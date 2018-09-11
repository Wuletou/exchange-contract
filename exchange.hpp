#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <boost/container/flat_map.hpp>
#include "exchange_state.hpp"
#include "whitelisted.hpp"

namespace eosio {

    class exchange : public whitelisted {
    public:
        exchange(account_name self) : whitelisted(self) {}

        struct spec_trade {
            uint64_t id;
            account_name seller;
            extended_asset sell;
            extended_asset receive;
        };

        struct market_trade {
            account_name seller;
            extended_symbol sell_symbol;
            extended_asset receive;
        };

        struct limit_trade {
            account_name seller;
            extended_asset sell;
            extended_symbol receive_symbol;
        };

        struct cancelx {
            uint64_t id;
            extended_symbol base_symbol;
            extended_symbol quote_symbol;
        };

        struct createx {
            account_name creator;
            extended_asset base_deposit;
            extended_asset quote_deposit;
        };

        void on(const createx &c);

        void on(const spec_trade &t);

        void on(const market_trade &t);

        void on(const limit_trade &t);

        void on(const cancelx &c);

        void apply(account_name contract, account_name act);

        extended_asset convert(extended_asset from, extended_symbol to) const;

    private:
        void _allowclaim(account_name owner, extended_asset quantity);

        void _claim(account_name owner,
                    account_name to,
                    extended_asset quantity);
    };
} // namespace eosio
