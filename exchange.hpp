#include <eosiolib/types.hpp>
#include <eosiolib/currency.hpp>
#include <boost/container/flat_map.hpp>
#include <cmath>
#include "exchange_accounts.hpp"

namespace eosio {

   class exchange {
      private:
         account_name      _this_contract;
         currency          _excurrencies;
         exchange_accounts _accounts;

      public:
         exchange( account_name self )
         :_this_contract(self),
          _excurrencies(self),
          _accounts(self)
         {}

         void createx( account_name    creator,
                       extended_asset  base_deposit,
                       extended_asset  quote_deposit
                     );

         void cancelx( uint64_t pk_value );
         void deposit( account_name from, extended_asset quantity );
         void withdraw( account_name  from, extended_asset quantity );

         struct trade {
            account_name    seller;
            extended_asset  sell;
            extended_asset  receive;
         };

         void on( const trade& t    );
         void on( const currency::transfer& t, account_name code );
         void apply( account_name contract, account_name act );
         extended_asset convert( extended_asset from, extended_symbol to ) const;
   };
} // namespace eosio
