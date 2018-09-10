#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <boost/container/flat_map.hpp>
#include "exchange_state.hpp"
#include "whitelisted.hpp"

namespace eosio {

    class exchange : public whitelisted {
    public:
        exchange(account_name self)
                : whitelisted(self),
                  orders(self, self) {}

        markets orders;

        struct trade {
            uint64_t pk_value;
            account_name seller;
            extended_asset sell;
            extended_asset receive;
        };

        void createx(account_name creator,
                     extended_asset base_deposit,
                     extended_asset quote_deposit);

        void cancelx(uint64_t pk_value);

        void on(const trade &t);

        void apply(account_name contract, account_name act);

        extended_asset convert(extended_asset from, extended_symbol to) const;

    private:
        void _allowclaim(account_name owner, extended_asset quantity);

        void _claim(account_name owner,
                    account_name to,
                    extended_asset quantity);
    };
} // namespace eosio
