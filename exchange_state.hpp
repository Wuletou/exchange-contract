#pragma once

#include <eosiolib/asset.hpp>
#include "pow10.h"

namespace eosio {

    struct pair_t {
        uint64_t id;
        symbol_type base_symbol;
        symbol_type quote_symbol;

        uint64_t primary_key() const { return id; }

        EOSLIB_SERIALIZE(pair_t, (id)(base_symbol)(quote_symbol))
    };

    typedef multi_index<N(pairs), pair_t> pairs_table;

    struct exchange_state {
        uint64_t id;
        account_name manager;
        asset base;
        symbol_type quote_symbol;
        double price;

        uint64_t primary_key() const { return id; }

        account_name get_manager() const { return manager; };

        double get_price() const { return price; }

        double get_rprice() const { return 1 / price; }

        extended_asset convert(extended_asset from, extended_symbol to_symbol) const;

        void print() const;

        EOSLIB_SERIALIZE(exchange_state, (id)(manager)(base)(quote_symbol)(price))
    };

    typedef eosio::multi_index<N(markets), exchange_state,
            indexed_by<N(byprice), const_mem_fun < exchange_state, double, &exchange_state::get_price> >
    > markets_table;

} /// namespace eosio
