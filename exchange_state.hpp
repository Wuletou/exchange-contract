#pragma once

#include <eosiolib/asset.hpp>
#include "pow10.h"

namespace eosio {

   struct exchange_state {
      account_name   manager;
      extended_asset base;
      extended_asset quote;
      double         price;

      exchange_state() { }

      exchange_state(account_name manager, extended_asset base, extended_asset quote) {
         this->manager = manager;
         this->base = base;
         this->quote = quote;
         this->price = (double) base.amount / quote.amount;
      }

      uint64_t primary_key() const { return manager + base.symbol.name() + quote.symbol.name() + price * POW10(15); }

      account_name get_manager() const { return manager; };
      double get_price() const { return price; }
      double get_rprice() const { return 1 / price; }

      extended_asset convert( extended_asset from, extended_symbol to_symbol );
      void print() const;

      EOSLIB_SERIALIZE( exchange_state, (manager)(base)(quote)(price) )
   };

   typedef eosio::multi_index<N(markets), exchange_state,
      indexed_by< N(byprice),  const_mem_fun< exchange_state, double, &exchange_state::get_price > >,
      indexed_by< N(byrprice), const_mem_fun< exchange_state, double, &exchange_state::get_rprice > >,
      indexed_by< N(bymanager),const_mem_fun< exchange_state, account_name, &exchange_state::get_manager > >
   > markets;

} /// namespace eosio
