#pragma once

#include <eosiolib/asset.hpp>
#include "pow10.h"

namespace eosio {

   struct exchange_state {
      account_name   manager;
      extended_asset base;
      extended_asset quote;

      static uint64_t create_primary_key( exchange_state ex ) {
          return ex.manager + ex.base.symbol.name() + ex.quote.symbol.name() + ex.base_rate() * POW10(15);
      }
      static uint64_t create_primary_key( account_name manager, extended_asset base, extended_asset quote ) {
          return create_primary_key(exchange_state{manager, base, quote});
      }
      uint64_t primary_key() const { return create_primary_key(manager, base, quote); }

      account_name get_manager() const { return manager; };
      double base_rate() const { return base.amount * 1. / quote.amount; }
      double quote_rate() const { return quote.amount * 1. / base.amount; }

//      double get_base_div_quote()const { return (real_type) base.amount / quote.amount; }
//      double get_quote_div_base()const { return (real_type) quote.amount / base.amount; }

//      extended_asset convert_to_exchange( connector& c, extended_asset in );
//      extended_asset convert_from_exchange( connector& c, extended_asset in );
      extended_asset convert( extended_asset from, extended_symbol to_symbol );
      void print() const;

      EOSLIB_SERIALIZE( exchange_state, (manager)(base)(quote) )
   };

   typedef eosio::multi_index<N(markets), exchange_state,
      indexed_by< N(bybaserate), const_mem_fun< exchange_state, double, &exchange_state::base_rate > >,
      indexed_by< N(byquoterate),const_mem_fun< exchange_state, double, &exchange_state::quote_rate > >,
      indexed_by< N(bymanager),  const_mem_fun< exchange_state, account_name, &exchange_state::get_manager > >
   > markets;

} /// namespace eosio
