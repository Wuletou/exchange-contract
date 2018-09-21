#pragma once

#include <eosiolib/eosio.hpp>

namespace eosio {

    class whitelisted : public contract {
    public:
        whitelisted(account_name self)
                : contract(self),
                  whitelist(self, self) {}

        struct whitelist {
            account_name account;

            uint64_t primary_key() const { return account; }
        };

        multi_index<N(whitelist), whitelist> whitelist;

        void white(account_name account);

        void unwhite(account_name account);

        void whitemany(vector<account_name> accounts);

        void unwhitemany(vector<account_name> accounts);
    protected:
        void setwhite(account_name account);

        void unsetwhite(account_name account);

        bool is_whitelisted(account_name account) { return whitelist.find(account) != whitelist.end(); }
    };
} // namespace eosio
