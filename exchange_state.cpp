#include "exchange_state.hpp"

namespace eosio {

   extended_asset exchange_state::convert( extended_asset from, extended_symbol to_symbol ) {
      auto from_symbol  = from.get_extended_symbol();
      auto base_symbol  = base.get_extended_symbol();
      auto quote_symbol = quote.get_extended_symbol();

      extended_asset out;

      if (from_symbol == base_symbol && to_symbol == quote_symbol) {
         out = extended_asset( from.amount * get_price(), to_symbol );
      } else if (from_symbol == quote_symbol && to_symbol == base_symbol) {
         out = extended_asset( from.amount * get_rprice(), to_symbol );
      } else {
         eosio_assert( false, "invalid conversion" );
      }

      return out;
   }

   void exchange_state::print() const {
      eosio::print(
         name{manager}, ' ',
         (asset) base, "->",
         (asset) quote, ' ',
         get_price(), ' ',
         get_rprice(), ' ',
         primary_key()
      );
   }

} /// namespace eosio
