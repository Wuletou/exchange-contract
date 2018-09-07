#include "whitelisted.hpp"

namespace eosio {

    void whitelisted::setwhite(account_name account) {
        auto it = whitelist.find(account);
        eosio_assert(it == whitelist.end(), "Account already whitelisted");
        whitelist.emplace(_self, [account](auto& e) {
            e.account = account;
        });
    }

    void whitelisted::unsetwhite(account_name account) {
        auto it = whitelist.find(account);
        eosio_assert(it != whitelist.end(), "Account not whitelisted");
        whitelist.erase(it);
    }

    void whitelisted::white(account_name account) {
        require_auth(_self);
        setwhite(account);
    }

    void whitelisted::unwhite(account_name account) {
        require_auth(_self);
        unsetwhite(account);
    }

    void whitelisted::whitemany(vector<account_name> accounts) {
        require_auth(_self);
        for (auto account : accounts) {
            setwhite(account);
        }
    }

    void whitelisted::unwhitemany(vector<account_name> accounts) {
        require_auth(_self);
        for (auto account : accounts) {
            unsetwhite(account);
        }
    }
}