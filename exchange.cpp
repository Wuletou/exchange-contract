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

   void exchange::on( const trade& t ) {
      require_auth( t.seller );
      eosio_assert( t.sell.is_valid(), "invalid sell amount" );
      eosio_assert( t.sell.amount > 0, "sell amount must be positive" );
      eosio_assert( t.receive.is_valid(), "invalid receive amount" );
      eosio_assert( t.receive.amount >= 0, "receive amount cannot be negative" );

      auto receive_symbol = t.receive.get_extended_symbol();
      eosio_assert( t.sell.get_extended_symbol() != receive_symbol, "invalid conversion" );
      // todo: check that sender's balance is enough

      market_state market( _this_contract, t.market, _accounts );

      auto temp   = market.exstate;
      auto output = temp.convert( t.sell, receive_symbol );

      while( temp.requires_margin_call() ) {
         market.margin_call( receive_symbol );
         temp = market.exstate;
         output = temp.convert( t.sell, receive_symbol );
      }
      market.exstate = temp;

      print( name{t.seller}, "   ", t.sell, "  =>  ", output, "\n" );

      if( t.receive.amount != 0 ) {
         eosio_assert( t.receive.amount <= output.amount, "unable to fill" );
      }

      _accounts.adjust_balance( t.seller, -t.sell, "sold" );
      _accounts.adjust_balance( t.seller, output, "received" );

      if( market.exstate.supply.amount != market.initial_state().supply.amount ) {
         auto delta = market.exstate.supply - market.initial_state().supply;

         _excurrencies.issue_currency( { .to = _this_contract,
                                               .quantity = delta,
                                               .memo = string("") } );
      }

      /// TODO: if pending order start deferred trx to fill it

      market.save();

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
