
mkdir release
strip explorer/explorer-node

cp explorer/explorer-node release/
cp explorer/explorer-node.cfg release/

strip litecash/litecash-node
cp litecash/litecash-node* release/

strip wallet/wallet-api
cp wallet/wallet-api* release/

strip wallet/litecash-wallet
cp wallet/litecash-wallet release/
cp wallet/litecash-wallet.cfg release/

cd release/


