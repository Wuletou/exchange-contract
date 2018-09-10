#include "exchange.hpp"
#include "exchange_state.cpp"
#include "whitelisted.cpp"

#include <eosiolib/dispatcher.hpp>

namespace eosio {

    extended_asset min_asset(extended_asset a, extended_asset b) {
        return a < b ? a : b;
    }

    void exchange::on(const trade &t) {
        require_auth(t.seller);
        eosio_assert(is_whitelisted(t.seller), "Account is not whitelisted");
        eosio_assert(t.sell.is_valid(), "invalid sell amount");
        eosio_assert(t.receive.is_valid(), "invalid receive amount");
        eosio_assert(t.sell.get_extended_symbol() != t.receive.get_extended_symbol(), "invalid exchange");

        if (t.pk_value != 0) {
            // apply a specific order
            auto existing = orders.find(t.pk_value);
            eosio_assert(existing != orders.end(), "Order with the specified primary key doesn't exist");
            eosio_assert(existing->base == t.receive, "Base deposits must be the same");
            eosio_assert(existing->quote == t.sell, "Base deposits must be the same");

            orders.erase(existing);

            _allowclaim(t.seller, t.sell);
            _claim(t.seller, existing->manager, t.sell);
            _claim(existing->manager, t.seller, t.receive);
        } else if (t.sell.amount == 0) {
            // market order: get X receive (base) for any sell (quote)
            eosio_assert(t.receive.amount > 0, "receive amount must be positive");
            extended_asset sold = extended_asset(0, t.sell.get_extended_symbol());
            extended_asset received = extended_asset(0, t.receive.get_extended_symbol());
            extended_asset estimated_to_receive;

            auto sorted_orders = orders.get_index<N(byprice)>();
            auto order_itr = sorted_orders.cbegin();
            while (order_itr != sorted_orders.cend()) {
                auto order = *order_itr;
                if (order.manager == t.seller
                    || order.quote.get_extended_symbol() != t.sell.get_extended_symbol()
                    || order.base.get_extended_symbol() != t.receive.get_extended_symbol()) {
                    order_itr++;
                    continue;
                }
                estimated_to_receive = t.receive - received;
                auto min = min_asset(order.base, estimated_to_receive);
                received += min;
                extended_asset output = order.convert(min, order.quote.get_extended_symbol());
                sold += output;

                if (min == order.base) {
                    order_itr = sorted_orders.erase(order_itr);
                } else if (min < order.base) {
                    sorted_orders.modify(order_itr, _self, [&](auto &s) {
                        s.base -= min;
                        s.quote -= output;
                    });
                } else {
                    eosio_assert(false, "incorrect state");
                }

                _allowclaim(t.seller, output);
                _claim(order.manager, t.seller, min);
                _claim(t.seller, order.manager, output);

                if (received == t.receive) break;
            }

            eosio_assert(received == t.receive, "unable to fill");
        } else if (t.receive.amount == 0) {
            // limit order: get maximum receive (base) for X sell (quote)
            eosio_assert(t.sell.amount > 0, "sell amount must be positive");
            extended_asset sold = extended_asset(0, t.sell.get_extended_symbol());
            extended_asset received = extended_asset(0, t.receive.get_extended_symbol());
            extended_asset estimated_to_sold;

            auto sorted_orders = orders.get_index<N(byprice)>();
            auto order_itr = sorted_orders.cbegin();
            while (order_itr != sorted_orders.cend()) {
                auto order = *order_itr;
                if (order.manager == t.seller
                    || order.quote.get_extended_symbol() != t.sell.get_extended_symbol()
                    || order.base.get_extended_symbol() != t.receive.get_extended_symbol()) {
                    order_itr++;
                    continue;
                }
                estimated_to_sold = t.sell - sold;
                auto min = min_asset(order.quote, estimated_to_sold);
                sold += min;
                extended_asset output = order.convert(min, order.base.get_extended_symbol());
                received += output;

                if (min == order.quote) {
                    order_itr = sorted_orders.erase(order_itr);
                } else if (min < order.quote) {
                    sorted_orders.modify(order_itr, _self, [&](auto &s) {
                        s.base -= output;
                        s.quote -= min;
                    });
                } else {
                    eosio_assert(false, "incorrect state");
                }

                _allowclaim(t.seller, min);
                _claim(t.seller, order.manager, min);
                _claim(order.manager, t.seller, output);

                if (sold == t.sell) break;
            }

            eosio_assert(sold == t.sell, "unable to fill");
        }
    }

    void exchange::createx(account_name creator,
                           extended_asset base_deposit,
                           extended_asset quote_deposit) {
        require_auth(creator);
        eosio_assert(is_whitelisted(creator), "Account is not whitelisted");
        eosio_assert(base_deposit.is_valid(), "invalid base deposit");
        eosio_assert(base_deposit.amount > 0, "base deposit must be positive");
        eosio_assert(quote_deposit.is_valid(), "invalid quote deposit");
        eosio_assert(quote_deposit.amount > 0, "quote deposit must be positive");
        eosio_assert(base_deposit.get_extended_symbol() != quote_deposit.get_extended_symbol(),
                     "must exchange between two different currencies");

        _allowclaim(creator, base_deposit);

        exchange_state order = exchange_state(creator, base_deposit, quote_deposit);
        print("base: ", order.base.get_extended_symbol(), '\n');
        print("quote: ", order.quote.get_extended_symbol(), '\n');

        auto existing = orders.find(order.primary_key());

        if (existing == orders.end()) {
            print("create new trade\n");
            orders.emplace(creator, [&](auto &s) { s = order; });
        } else {
            print("combine trades with same rate\n");
            orders.modify(existing, _self, [&](auto &s) {
                s.base += base_deposit;
                s.quote += quote_deposit;
            });
        }
    }

    void exchange::cancelx(uint64_t pk_value) {
        auto order = orders.find(pk_value);
        eosio_assert(order != orders.end(), "order doesn't exist");
        require_auth(order->manager);
        orders.erase(order);
    }

    void exchange::_allowclaim(account_name owner, extended_asset quantity) {
        struct allowclaim {
            account_name from;
            asset quantity;
        };

        action(eosio::vector<permission_level>({
                   permission_level(_self, N(active)),
                   permission_level(owner, N(active))
               }),
               quantity.contract,
               N(allowclaim),
               allowclaim{owner, quantity}).send();
    }

    void exchange::_claim(account_name owner,
                          account_name to,
                          extended_asset quantity) {
        struct claim {
            account_name from;
            account_name to;
            asset quantity;
        };

        action(eosio::vector<permission_level>({
                   permission_level(_self, N(active)),
                   permission_level(to, N(active))
               }),
               quantity.contract,
               N(claim),
               claim{owner, to, quantity}).send();
    };

    void exchange::apply(account_name contract, account_name act) {
        if (contract != _self)
            return;

        auto &thiscontract = *this;
        switch (act) {
            EOSIO_API(exchange, (createx)(cancelx)(white)(unwhite)(whitemany)(unwhitemany))
        };

        switch (act) {
            case N(trade):
                on(unpack_action_data<trade>());
                return;
        }
    }
} /// namespace eosio

extern "C" {
    [[noreturn]] void apply(uint64_t receiver,
                            uint64_t code,
                            uint64_t action) {
        eosio::exchange ex(receiver);
        ex.apply(code, action);
        eosio_exit(0);
    }
}
