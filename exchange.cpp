#include <math.h>
#include "exchange.hpp"

#include "exchange_state.cpp"
#include "exchange_accounts.cpp"

#include <eosiolib/dispatcher.hpp>

namespace eosio {

   void exchange::deposit( account_name from, extended_asset quantity ) {
      eosio_assert( quantity.is_valid(), "invalid quantity" );
      currency::inline_transfer( from, _this_contract, quantity, "deposit" );
      _accounts.adjust_balance( from, quantity, "deposit" );
   }

   void exchange::withdraw( account_name from, extended_asset quantity ) {
      require_auth( from );
      eosio_assert( quantity.is_valid(), "invalid quantity" );
      eosio_assert( quantity.amount >= 0, "cannot withdraw negative balance" );
      _accounts.adjust_balance( from, -quantity );
      currency::inline_transfer( _this_contract, from, quantity, "withdraw" );
   }

   extended_asset min_asset(extended_asset a, extended_asset b) {
      return a < b ? a : b;
   }

   void exchange::on( const trade& t ) {
      require_auth( t.seller );
      eosio_assert( t.sell.is_valid(), "invalid sell amount" );
      eosio_assert( t.receive.is_valid(), "invalid receive amount" );
      eosio_assert( t.sell.get_extended_symbol() != t.receive.get_extended_symbol(), "invalid exchange" );

      markets orders( _this_contract, _this_contract );
      auto sorted_orders = orders.get_index<N(byprice)>();

      if (t.sell.amount == 0) {
         eosio_assert( t.receive.amount > 0, "receive amount must be positive" );
         // market order: get X receive (base) for any sell (quote)
         extended_asset sold = extended_asset(0, t.sell.get_extended_symbol());
         extended_asset received = extended_asset(0, t.receive.get_extended_symbol());
         extended_asset estimated_to_receive;

         auto order_itr = sorted_orders.cbegin();
         while (order_itr != sorted_orders.cend()) {
            auto order = *order_itr;
            if (order.manager == t.seller || order.quote.get_extended_symbol() != t.sell.get_extended_symbol()
                                          || order.base.get_extended_symbol() != t.receive.get_extended_symbol()) {
               order_itr++;
               continue;
            }
            estimated_to_receive = t.receive - received;
            auto min = min_asset(order.base, estimated_to_receive);
            received += min;
            extended_asset output = order.convert(min, order.quote.get_extended_symbol());
            sold += output;

            if (min == order.base ) {
               order_itr = sorted_orders.erase(order_itr);
            } else if (min < order.base) {
               sorted_orders.modify(order_itr, _this_contract, [&]( auto& s ) {
                  s.base -= min;
                  s.quote -= output;
               });
            } else {
               eosio_assert( false, "incorrect state" );
            }

            _accounts.adjust_balance( order.manager, -min, "sold" );
            _accounts.adjust_balance( order.manager, output, "receive" );

            if ( received == t.receive ) break;
         }

         eosio_assert( received == t.receive, "unable to fill");

         _accounts.adjust_balance( t.seller, -sold, "sold" );
         _accounts.adjust_balance( t.seller, received, "received" );
      } else if (t.receive.amount == 0) {
         eosio_assert( t.sell.amount > 0, "sell amount must be positive" );
         // limit order: get maximum receive (base) for X sell (quote)
         extended_asset sold = extended_asset(0, t.sell.get_extended_symbol());
         extended_asset received = extended_asset(0, t.receive.get_extended_symbol());
         extended_asset estimated_to_sold;

         auto order_itr = sorted_orders.cbegin();
         while (order_itr != sorted_orders.cend()) {
            auto order = *order_itr;
            if (order.manager == t.seller || order.quote.get_extended_symbol() != t.sell.get_extended_symbol()
                                          || order.base.get_extended_symbol() != t.receive.get_extended_symbol()) {
               order_itr++;
               continue;
            }
            estimated_to_sold = t.sell - sold;
            auto min = min_asset(order.quote, estimated_to_sold);
            sold += min;
            extended_asset output = order.convert(min, order.base.get_extended_symbol());
            received += output;

            if (min == order.quote ) {
               order_itr = sorted_orders.erase(order_itr);
            } else if (min < order.quote) {
               sorted_orders.modify( order_itr, _this_contract, [&]( auto& s ) {
                  s.base -= output;
                  s.quote -= min;
               });
            } else {
               eosio_assert( false, "incorrect state" );
            }

            _accounts.adjust_balance( order.manager, -min, "sold" );
            _accounts.adjust_balance( order.manager, output, "receive" );

            if ( sold == t.sell ) break;
         }

         eosio_assert( sold == t.sell, "unable to fill");

         _accounts.adjust_balance( t.seller, -sold, "sold" );
         _accounts.adjust_balance( t.seller, received, "received" );
      }
   }

   void exchange::createx( account_name    creator,
                 extended_asset  base_deposit,
                 extended_asset  quote_deposit
               ) {
      require_auth( creator );
      eosio_assert( base_deposit.is_valid(), "invalid base deposit" );
      eosio_assert( base_deposit.amount > 0, "base deposit must be positive" );
      eosio_assert( quote_deposit.is_valid(), "invalid quote deposit" );
      eosio_assert( quote_deposit.amount > 0, "quote deposit must be positive" );
      eosio_assert( base_deposit.get_extended_symbol() != quote_deposit.get_extended_symbol(),
                    "must exchange between two different currencies" );
      // todo: check that sender's balance is enough

      exchange_state order = exchange_state(creator, base_deposit, quote_deposit);
      print( "base: ", order.base.get_extended_symbol(), '\n');
      print( "quote: ", order.quote.get_extended_symbol(), '\n');

      markets orders( _this_contract, _this_contract );
      auto existing = orders.find( order.primary_key() );

      if (existing == orders.end()) {
         print("create new trade\n");
         orders.emplace( creator, [&]( auto& s ) { s = order; } );
      } else {
         print("combine trades with same rate\n");
         orders.modify(existing, _this_contract, [&]( auto& s ) {
            s.base += base_deposit;
            s.quote += quote_deposit;
         });
      }
   }

   void exchange::cancelx( uint64_t pk_value ) {
      markets orders( _this_contract, _this_contract );
      auto order = orders.find( pk_value );
      eosio_assert( order != orders.end(), "order doesn't exist" );
      require_auth( order->manager );
      orders.erase(order);
   }

   void exchange::on( const currency::transfer& t, account_name code ) {
      if( code == _this_contract )
         _excurrencies.on( t );

      if( t.to == _this_contract ) {
         auto a = extended_asset(t.quantity, code);
         eosio_assert( a.is_valid(), "invalid quantity in transfer" );
         eosio_assert( a.amount != 0, "zero quantity is disallowed in transfer");
         eosio_assert( a.amount > 0 || t.memo == "withdraw", "withdrew tokens without withdraw in memo");
         eosio_assert( a.amount < 0 || t.memo == "deposit", "received tokens without deposit in memo" );
         _accounts.adjust_balance( t.from, a, t.memo );
      }
   }


   #define N(X) ::eosio::string_to_name(#X)

   void exchange::apply( account_name contract, account_name act ) {

      if( act == N(transfer) ) {
         on( unpack_action_data<currency::transfer>(), contract );
         return;
      }

      if( contract != _this_contract )
         return;

      auto& thiscontract = *this;
      switch( act ) {
         EOSIO_API( exchange, (createx)(cancelx)(deposit)(withdraw) )
      };

      switch( act ) {
         case N(trade):
            on( unpack_action_data<trade>() );
            return;
         default:
            _excurrencies.apply( contract, act );
            return;
      }
   }

} /// namespace eosio



extern "C" {
   [[noreturn]] void apply( uint64_t receiver, uint64_t code, uint64_t action ) {
      eosio::exchange  ex( receiver );
      ex.apply( code, action );
      eosio_exit(0);
   }
}
