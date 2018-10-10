#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <boost/container/flat_map.hpp>
#include "exchange_state.hpp"
#include "whitelisted.hpp"
#include "str_expand.h"
#include "config.h"

namespace eosio {

    class exchange : public whitelisted {
    public:
        exchange(account_name self)
                : whitelisted(self)
                , wu_contract(string_to_name(STR(WU_ACCOUNT)))
                , wu_symbol(string_to_symbol(WU_DECIMALS, STR(WU_SYMBOL)))
                , loyalty_contract(string_to_name(STR(LT_ACCOUNT)))
                , lt_symbols(loyalty_contract, loyalty_contract) {}

        account_name wu_contract;
        symbol_type wu_symbol;
        account_name loyalty_contract;

        struct spec_trade {
            uint64_t id;
            account_name seller;
            asset sell;
            asset receive;
        };

        struct market_trade {
            account_name seller;
            symbol_type sell_symbol;
            asset receive;
        };

        struct limit_trade {
            account_name seller;
            asset sell;
            symbol_type receive_symbol;
        };

        struct cancelx {
            uint64_t id;
            symbol_type base_symbol;
            symbol_type quote_symbol;
        };

        struct createx {
            account_name creator;
            asset base_deposit;
            asset quote_deposit;
        };

        void on(const createx &c);

        void on(const spec_trade &t);

        void on(const market_trade &t);

        void on(const limit_trade &t);

        void on(const cancelx &c);

        void apply(account_name contract, account_name act);

        extended_asset convert(extended_asset from, extended_symbol to) const;

        void cleanstate();
    private:
        struct symbols_t {
            eosio::symbol_name symbol;
            uint64_t primary_key() const { return symbol; }
        };

        multi_index<N(symbols), symbols_t> lt_symbols;

        void _allowclaim(account_name owner, extended_asset quantity);

        void _claim(account_name owner,
                    account_name to,
                    extended_asset quantity);
    };
} // namespace eosio
