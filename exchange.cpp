#include "exchange.hpp"
#include "exchange_state.cpp"
#include "whitelisted.cpp"

#include <eosiolib/dispatcher.hpp>
#include <string>

namespace eosio {

    extended_asset min_asset(extended_asset a, extended_asset b) {
        return a < b ? a : b;
    }

    void exchange::on(const spec_trade &t) {
        require_auth(t.seller);
        eosio_assert(is_whitelisted(t.seller), "Account is not whitelisted");
        eosio_assert(t.sell_symbol.is_valid(), "invalid sell amount");
        eosio_assert(t.receive.is_valid(), "invalid receive amount");

        auto base_symbol = t.receive.symbol;
        auto quote_symbol = t.sell_symbol;
        eosio_assert(base_symbol != quote_symbol, "invalid exchange");

        auto pairs = pairs_table(_self, _self);
        auto existing_pair = pairs.end();
        for (auto itr = pairs.begin(); itr != pairs.end(); itr++) {
            if (itr->base_symbol == base_symbol && itr->quote_symbol == quote_symbol) {
                existing_pair = itr;
                break;
            }
        }
        eosio_assert(existing_pair != pairs.end(), "Pair doesn't exist");

        extended_symbol sell_symbol;
        extended_asset receive;
        if (t.sell_symbol == wu_symbol) {
            sell_symbol = extended_symbol(t.sell_symbol, wu_contract);
            receive = extended_asset(t.receive, loyalty_contract);
        } else {
            sell_symbol = extended_symbol(t.sell_symbol, loyalty_contract);
            receive = extended_asset(t.receive, wu_contract);
        }

        markets_table markets(_self, existing_pair->id);
        auto existing = markets.find(t.id);
        eosio_assert(existing != markets.end(), "Order with the specified primary key doesn't exist");
        eosio_assert(existing->base == receive, "Base deposits must be the same");
        eosio_assert(existing->quote_symbol == sell_symbol, "Base symbols must be the same");

        extended_asset sell = existing->convert(receive, sell_symbol);

        markets.erase(existing);

        _allowclaim(t.seller, sell);
        _claim(t.seller, existing->manager, sell);
        _claim(existing->manager, t.seller, receive);
    }

    void exchange::on(const market_trade &t) {
        // market order: get X receive (base) for any sell (quote)
        auto base_symbol = t.receive.symbol;
        auto quote_symbol = t.sell_symbol;

        require_auth(t.seller);
        eosio_assert(is_whitelisted(t.seller), "Account is not whitelisted");
        eosio_assert(t.receive.is_valid(), "invalid receive amount");
        eosio_assert(base_symbol != quote_symbol, "invalid exchange");

        auto pairs = pairs_table(_self, _self);
        auto existing_pair = pairs.end();
        for (auto pair = pairs.begin(); pair != pairs.end(); pair++) {
            if (pair->base_symbol == base_symbol && pair->quote_symbol == quote_symbol) {
                existing_pair = pair;
                break;
            }
        }
        eosio_assert(existing_pair != pairs.end(), "Pair doesn't exist");

        account_name base_contract = base_symbol == wu_symbol ? wu_contract : loyalty_contract;
        account_name quote_contract = quote_symbol == wu_symbol ? wu_contract : loyalty_contract;

        markets_table markets(_self, existing_pair->id);
        eosio_assert(t.receive.amount > 0, "receive amount must be positive");
        auto sold = asset(0, quote_symbol);
        auto received = asset(0, base_symbol);

        auto sorted_markets = markets.get_index<N(byprice)>();
        for (auto order = sorted_markets.begin(); order != sorted_markets.end(); ) {
            if (order->manager == t.seller || order->quote_symbol != quote_symbol || order->base.symbol != base_symbol) {
                order++;
                continue;
            }
            extended_asset estimated_to_receive = extended_asset(t.receive - received, base_contract);
            auto min = min_asset(extended_asset(order->base, base_contract), estimated_to_receive);
            received += min;
            extended_asset output = order->convert(min, extended_symbol(order->quote_symbol, quote_contract));
            sold += output;

            _allowclaim(t.seller, output);
            _claim(t.seller, order->manager, output);
            _claim(order->manager, t.seller, min);

            if (min == order->base) {
                order = sorted_markets.erase(order);
            } else if (min < order->base) {
                sorted_markets.modify(order, _self, [&](auto &s) {
                    s.base -= min;
                });
            } else {
                eosio_assert(false, "incorrect state");
            }

            if (received == t.receive) break;
        }

        eosio_assert(received == t.receive, "unable to fill");
    }

    void exchange::on(const limit_trade &t) {
        // limit order: get maximum receive (base) for X sell (quote)
        auto base_symbol = t.receive_symbol;
        auto quote_symbol = t.sell.symbol;

        require_auth(t.seller);
        eosio_assert(is_whitelisted(t.seller), "Account is not whitelisted");
        eosio_assert(t.sell.is_valid(), "invalid sell amount");
        eosio_assert(base_symbol != quote_symbol, "invalid exchange");

        auto pairs = pairs_table(_self, _self);
        auto existing_pair = pairs.end();
        for (auto pair = pairs.begin(); pair != pairs.end(); pair++) {
            if (pair->base_symbol == base_symbol && pair->quote_symbol == quote_symbol) {
                existing_pair = pair;
                break;
            }
        }
        eosio_assert(existing_pair != pairs.end(), "Pair doesn't exist");

        account_name base_contract = base_symbol == wu_symbol ? wu_contract : loyalty_contract;
        account_name quote_contract = quote_symbol == wu_symbol ? wu_contract : loyalty_contract;

        markets_table markets(_self, existing_pair->id);
        eosio_assert(t.sell.amount > 0, ("sell amount must be positive" + std::to_string(t.sell.amount)).c_str());
        auto sold = asset(0, quote_symbol);
        auto received = asset(0, base_symbol);

        auto sorted_markets = markets.get_index<N(byprice)>();
        for (auto order = sorted_markets.begin(); order != sorted_markets.end(); ) {
            if (order->manager == t.seller || order->quote_symbol != quote_symbol || order->base.symbol != base_symbol) {
                order++;
                continue;
            }
            extended_asset estimated_to_sold = extended_asset(t.sell - sold, quote_contract);
            auto quote = order->convert(extended_asset(order->base, base_contract), extended_symbol(order->quote_symbol, quote_contract));
            auto min = min_asset(extended_asset(quote, quote_contract), estimated_to_sold);
            sold += min;

            extended_asset output;
            if (min == quote) {
                output = extended_asset(order->base, base_contract);
            } else {
                output = order->convert(estimated_to_sold, extended_symbol(order->base.symbol, base_contract));
            }
//            extended_asset output = order->convert(min, extended_symbol(order->base.symbol, base_contract));
            received += output;

            print("min: ", min, "\n");
            print("output: ", output, "\n");

            _allowclaim(t.seller, min);
            _claim(t.seller, order->manager, min);
            _claim(order->manager, t.seller, output);

            if (min == quote) {
                order = sorted_markets.erase(order);
            } else if (min < quote) {
                sorted_markets.modify(order, _self, [&](auto &s) {
                    s.base -= output;
                });
            } else {
                eosio_assert(false, "incorrect state");
            }

            if (sold == t.sell) break;
        }

        eosio_assert(sold == t.sell, "unable to fill");
    }

    void exchange::on(const trade &t) {
        // get WU balance before trade
        auto pairs = pairs_table(_self, _self);
        auto existing_pair = pairs.end();
        for (auto pair = pairs.begin(); pair != pairs.end(); pair++) {
            if (pair->base_symbol == wu_symbol && pair->quote_symbol == t.sell.symbol) {
                existing_pair = pair;
                break;
            }
        }
        eosio_assert(existing_pair != pairs.end(), "Pair doesn't exist");
        auto markets = markets_table(_self, existing_pair->id);
        auto sorted_markets = markets.get_index<N(byprice)>();
        auto order = sorted_markets.begin();
        eosio_assert(order != sorted_markets.end(), "Markets doesn't exist");

        auto wu_amount = order->convert(extended_asset(t.sell, loyalty_contract), extended_symbol(wu_symbol, wu_contract));
        print("wu amount: ", wu_amount, '\n');

        print("trade 1\n");
        on(limit_trade{t.seller, t.sell, wu_symbol});

        print("trade 2\n");
        on(limit_trade{t.seller, wu_amount, t.receive_symbol});
    }

    void exchange::on(const createx &c) {
        require_auth(c.creator);

        auto base_symbol = c.base_deposit.symbol;
        auto quote_symbol = c.quote_deposit.symbol;
        extended_asset base_deposit;
        extended_asset quote_deposit;

        bool base_is_wu = base_symbol == wu_symbol;
        bool quote_is_wu = quote_symbol == wu_symbol;
        if (base_is_wu && !quote_is_wu) {
            eosio_assert(lt_symbols.find(quote_symbol) != lt_symbols.end(), "There is no such loyalty token");

            base_deposit = extended_asset(c.base_deposit, wu_contract);
            quote_deposit = extended_asset(c.quote_deposit, loyalty_contract);
        } else if (!base_is_wu && quote_is_wu) {
            eosio_assert(lt_symbols.find(base_symbol) != lt_symbols.end(), "There is no such loyalty token");

            base_deposit = extended_asset(c.base_deposit, loyalty_contract);
            quote_deposit = extended_asset(c.quote_deposit, wu_contract);
        } else {
            eosio_assert(false, "One of the tokens must be WU, another token of loyalty");
        }

        eosio_assert(is_whitelisted(c.creator), "Account is not whitelisted");
        eosio_assert(base_deposit.is_valid(), "invalid base deposit");
        eosio_assert(base_deposit.amount > 0, "base deposit must be positive");
        eosio_assert(quote_deposit.is_valid(), "invalid quote deposit");
        eosio_assert(quote_deposit.amount > 0, "quote deposit must be positive");

        // add pair if doesn't exist
        pairs_table pairs(_self, _self);
        auto existing_pair = pairs.end();
        for (auto itr = pairs.begin(); itr != pairs.end(); itr++) {
            if (itr->base_symbol == base_symbol && itr->quote_symbol == quote_symbol) {
                existing_pair = itr;
                break;
            }
        }
        if (existing_pair == pairs.end()) {
            auto id = pairs.available_primary_key();
            pairs.emplace(_self, [&](auto& p) {
                p.id = id;
                p.base_symbol = base_symbol;
                p.quote_symbol = quote_symbol;
            });
            existing_pair = pairs.find(id);
        }

        _allowclaim(c.creator, base_deposit);

        print("base: ", base_deposit.get_extended_symbol(), '\n');
        print("quote: ", quote_deposit.get_extended_symbol(), '\n');

        auto price = (double) (quote_deposit.amount * pow10(base_symbol.precision()))
                / (base_deposit.amount * pow10(quote_symbol.precision()));

        auto markets = markets_table(_self, existing_pair->id);
        auto existing = markets.end();
        for (auto itr = markets.begin(); itr != markets.end(); itr++) {
            if (itr->manager == c.creator && itr->price == price) {
                existing = itr;
                break;
            }
        }

        if (existing == markets.end()) {
            print("create new trade\n");
            markets.emplace(c.creator, [&](auto &s) {
                s.id = markets.available_primary_key();
                s.manager = c.creator;
                s.base = base_deposit;
                s.quote_symbol = quote_symbol;
                s.price = price;
            });
        } else {
            print("combine trades with same rate\n");
            markets.modify(existing, _self, [&](auto &s) {
                s.base += base_deposit;
            });
        }
    }

    void exchange::on(const cancelx &c) {
        pairs_table pairs(_self, _self);
        auto existing_pair = pairs.end();
        for (auto itr = pairs.begin(); itr != pairs.end(); itr++) {
            if (itr->base_symbol == c.base_symbol && itr->quote_symbol == c.quote_symbol) {
                existing_pair = itr;
                break;
            }
        }
        markets_table markets(_self, existing_pair->id);
        auto market = markets.find(c.id);
        eosio_assert(market != markets.end(), "order doesn't exist");

        require_auth(market->manager);
        account_name base_contract = c.base_symbol == wu_symbol ? wu_contract : loyalty_contract;
        _allowclaim(market->manager, extended_asset(-market->base, base_contract));
        markets.erase(market);
    }

    void exchange::cleanstate() {
        require_auth(this->_self);

        pairs_table pairs(_self, _self);
        for (auto pair = pairs.begin(); pair != pairs.end(); ) {
            markets_table markets(_self, pair->id);
            for (auto market = markets.begin(); market != markets.end(); ) {
                market = markets.erase(market);
            }

            pair = pairs.erase(pair);
        }

        for (auto account = whitelist.begin(); account != whitelist.end(); ) {
            account = whitelist.erase(account);
        }
    }

    void exchange::_allowclaim(account_name owner, extended_asset quantity) {
        struct allowclaim {
            account_name from;
            asset quantity;
        };

        action(eosio::vector<permission_level>{
                   permission_level(owner, N(active)),
                   permission_level(_self, N(active))
               },
               quantity.contract,
               N(allowclaim),
               allowclaim{owner, quantity}).send();
    }

    void exchange::_claim(account_name owner,
                          account_name to,
                          extended_asset quantity) {
        struct claim {
            account_name from;
            asset quantity;
        };

        struct transfer {
            account_name from;
            account_name to;
            asset quantity;
            std::string memo;
        };

        action(permission_level(_self, N(active)),
               quantity.contract,
               N(claim),
               claim{owner, quantity}).send();

        action(permission_level(_self, N(active)),
               quantity.contract,
               N(transfer),
               transfer{_self, to, quantity, "claim"}).send();
    };

    void exchange::apply(account_name contract, account_name act) {
        if (contract != _self)
            return;

        auto &thiscontract = *this;
        switch (act) {
            EOSIO_API(exchange, (white)(unwhite)(whitemany)(unwhitemany)(cleanstate))
        };

        switch (act) {
            case N(createx):
                on(unpack_action_data<createx>());
                return;
            case N(spec.trade):
                on(unpack_action_data<spec_trade>());
                return;
            case N(market.trade):
                on(unpack_action_data<market_trade>());
                return;
            case N(limit.trade):
                on(unpack_action_data<limit_trade>());
                return;
            case N(trade):
                on(unpack_action_data<trade>());
                return;
            case N(cancelx):
                on(unpack_action_data<cancelx>());
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
