#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <boost/container/flat_map.hpp>

namespace eosio {

    class exchange : public contract {
    public:
        exchange(account_name self)
                : contract(self),
                  whitelist(self, self) {}

        void createx(account_name creator,
                     extended_asset base_deposit,
                     extended_asset quote_deposit);

        void cancelx(uint64_t pk_value);

        struct trade {
            account_name seller;
            extended_asset sell;
            extended_asset receive;
        };

        struct whitelist {
            account_name account;

            uint64_t primary_key() const { return account; }
        };

        multi_index<N(whitelist), whitelist> whitelist;

        void on(const trade &t);

        void apply(account_name contract, account_name act);

        extended_asset convert(extended_asset from, extended_symbol to) const;

        void white(account_name account);

        void unwhite(account_name account);

        void whitemany(vector<account_name> accounts);

        void unwhitemany(vector<account_name> accounts);

    private:
        account_name _this_contract;

        void _allowclaim(account_name owner, extended_asset quantity);

        void _claim(account_name owner,
                    account_name to,
                    extended_asset quantity);

        void setwhite(account_name account);

        void unsetwhite(account_name account);

        bool is_whitelisted(account_name account) { return whitelist.find(account) != whitelist.end(); }
    };
} // namespace eosio
