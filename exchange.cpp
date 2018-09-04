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

      markets exstates( _this_contract, _this_contract );
      for (auto exstate : exstates.get_index<N(byquoterate)>()) {
         exstate.print();
      }

      auto indexed_exstates = exstates.get_index<N(byquoterate)>();
      if (t.sell.amount == 0) {
         eosio_assert( t.receive.amount > 0, "receive amount must be positive" );
         // market order: get X receive (base) for any sell (quote)
         extended_asset sold = extended_asset(0, t.sell.get_extended_symbol());
         extended_asset received = extended_asset(0, t.receive.get_extended_symbol());
         extended_asset estimated_to_receive;
         for (auto ex_itr = indexed_exstates.cbegin(); ex_itr != indexed_exstates.cend(); ex_itr++) {
            auto ex = *ex_itr;
            estimated_to_receive = t.receive - received;
            auto min = min_asset(ex.base, estimated_to_receive);
            received += min;
            extended_asset output = ex.convert(min, ex.quote.get_extended_symbol());

            if (min == ex.base ) {
               indexed_exstates.erase(ex_itr);
            } else if (min < ex.base) {
               indexed_exstates.modify(ex_itr, _this_contract, [&]( auto& s ) {
                  s.base -= min;
                  s.quote += output;
               });
               break;
            } else {
               eosio_assert( false, "incorrect state" ); // todo: remove
            }

            _accounts.adjust_balance( ex.manager, -min, "sold" );
            _accounts.adjust_balance( ex.manager, output, "receive" );
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
         for (auto ex_itr = indexed_exstates.cbegin(); ex_itr != indexed_exstates.cend(); ex_itr++) {
            auto ex = *ex_itr;
            estimated_to_sold = t.sell - sold;
            auto min = min_asset(ex.quote, estimated_to_sold);
            sold += min;
            extended_asset output = ex.convert(min, ex.base.get_extended_symbol());

            if (min == ex.quote ) {
               indexed_exstates.erase(ex_itr);
            } else if (min < ex.quote) {
               indexed_exstates.modify( ex_itr, _this_contract, [&]( auto& s ) {
                    s.base -= output;
                    s.quote += min;
                });
                break;
            } else {
                eosio_assert( false, "incorrect state" ); // todo: remove
            }

            _accounts.adjust_balance( ex.manager, -min, "sold" );
            _accounts.adjust_balance( ex.manager, output, "receive" );
         }

         eosio_assert( sold == t.sell, "unable to fill");

         _accounts.adjust_balance( t.seller, -sold, "sold" );
         _accounts.adjust_balance( t.seller, received, "received" );
      }

      // todo: check that sender's balance is enough
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

      print( "base: ", base_deposit.get_extended_symbol(), '\n');
      print( "quote: ",quote_deposit.get_extended_symbol(), '\n');

      markets exstates( _this_contract, _this_contract );

      auto existing = exstates.find( exchange_state::create_primary_key(creator, base_deposit, quote_deposit) );

      if (existing == exstates.end()) {
         print("create new trade\n");
         exstates.emplace( creator, [&]( auto& s ) {
            s.manager = creator;
            s.base = base_deposit;
            s.quote = quote_deposit;
         });
      } else {
         print("combine trades with same rate\n");
         exstates.modify(existing, _this_contract, [&]( auto& s ) {
            s.base += base_deposit;
            s.quote += quote_deposit;
         });
      }
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
         EOSIO_API( exchange, (createx)(deposit)(withdraw) )
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
