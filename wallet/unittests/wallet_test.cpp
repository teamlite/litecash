// Copyright 2018 The Beam Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LOG_VERBOSE_ENABLED
    #define LOG_VERBOSE_ENABLED 0
#endif

#include "wallet/common.h"
#include "wallet/wallet_network.h"
#include "wallet/wallet.h"
#include "wallet/secstring.h"
#include "utility/test_helpers.h"
#include "../../core/radixtree.h"
#include "../../core/unittest/mini_blockchain.h"
#include <string_view>
#include "wallet/wallet_transaction.h"

#include "test_helpers.h"

#include <assert.h>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

#include "core/proto.h"
#include <boost/filesystem.hpp>
#include <boost/intrusive/list.hpp>

using namespace beam;
using namespace std;
using namespace ECC;

WALLET_TEST_INIT

Coin CreateAvailCoin(Amount amount, Height maturity = 10)
{
    Coin c(amount);
    c.m_maturity = maturity;
    c.m_confirmHeight = maturity;
    return c;
}

namespace
{
    class BaseTestWalletDB : public IWalletDB
    {
        Key::IKdf::Ptr m_pKdf;
        Block::SystemState::HistoryMap m_Hist;
        uint64_t m_KeyIndex = 1;
    public:

        BaseTestWalletDB()
        {
            uintBig seed;
            seed = 10U;
            HKdf::Create(m_pKdf, seed);
        }


        Key::IKdf::Ptr get_MasterKdf() const override
        {
            return m_pKdf;
        }

        std::vector<beam::Coin> selectCoins(ECC::Amount amount) override
        {
            std::vector<beam::Coin> res;
            ECC::Amount t = 0;
            for (auto& c : m_coins)
            {
                t += c.m_ID.m_Value;
                c.m_status = Coin::Outgoing;
                res.push_back(c);
                if (t >= amount)
                {
                    break;
                }
            }
            return res;
        }

        uint64_t AllocateKidRange(uint64_t nCount) override
        {
            uint64_t ret = m_KeyIndex;
            m_KeyIndex += nCount;
            return ret;
        }
        bool find(Coin& coin) override { return false; }
        std::vector<beam::Coin> getCoinsCreatedByTx(const TxID& txId) override { return {}; };
        std::vector<Coin> getCoinsByID(const CoinIDList& ids) override { return {}; };
        void store(beam::Coin& ) override {}
        void store(std::vector<beam::Coin>&) override {}
        void save(const beam::Coin&) override {}
        void save(const std::vector<beam::Coin>& ) override {}
        void remove(const std::vector<beam::Coin::ID>&) override {}
        void remove(const beam::Coin::ID&) override {}
        void visit(std::function<bool(const beam::Coin& coin)> ) override {}
        void setVarRaw(const char* , const void* , size_t ) override {}
        bool getVarRaw(const char* , void* , int) const override { return false; }
        bool getBlob(const char* name, ByteBuffer& var) const override { return false; }
        Timestamp getLastUpdateTime() const override { return 0; }
        void setSystemStateID(const Block::SystemState::ID& ) override {};
        bool getSystemStateID(Block::SystemState::ID& ) const override { return false; };

        void subscribe(IWalletDbObserver* observer) override {}
        void unsubscribe(IWalletDbObserver* observer) override {}

        std::vector<TxDescription> getTxHistory(uint64_t , int ) override { return {}; };
        boost::optional<TxDescription> getTx(const TxID& ) override { return boost::optional<TxDescription>{}; };
        void saveTx(const TxDescription& p) override
        {
            setTxParameter(p.m_txId, wallet::TxParameterID::Amount, wallet::toByteBuffer(p.m_amount), false);
            setTxParameter(p.m_txId, wallet::TxParameterID::Fee, wallet::toByteBuffer(p.m_fee), false);
            setTxParameter(p.m_txId, wallet::TxParameterID::Change, wallet::toByteBuffer(p.m_change), false);
            setTxParameter(p.m_txId, wallet::TxParameterID::MinHeight, wallet::toByteBuffer(p.m_minHeight), false);
            setTxParameter(p.m_txId, wallet::TxParameterID::PeerID, wallet::toByteBuffer(p.m_peerId), false);
            setTxParameter(p.m_txId, wallet::TxParameterID::MyID, wallet::toByteBuffer(p.m_myId), false);
            setTxParameter(p.m_txId, wallet::TxParameterID::Message, wallet::toByteBuffer(p.m_message), false);
            setTxParameter(p.m_txId, wallet::TxParameterID::CreateTime, wallet::toByteBuffer(p.m_createTime), false);
            setTxParameter(p.m_txId, wallet::TxParameterID::ModifyTime, wallet::toByteBuffer(p.m_modifyTime), false);
            setTxParameter(p.m_txId, wallet::TxParameterID::IsSender, wallet::toByteBuffer(p.m_sender), false);
            setTxParameter(p.m_txId, wallet::TxParameterID::Status, wallet::toByteBuffer(p.m_status), false);
        };
        void deleteTx(const TxID& ) override {};
        void rollbackTx(const TxID&) override {}

        std::vector<WalletAddress> getAddresses(bool own) const override { return {}; }

        WalletAddress m_LastAdddr;

        void saveAddress(const WalletAddress& wa) override
        {
            m_LastAdddr = wa;
        }

        void setExpirationForAllAddresses(uint64_t expiration) override {};
        boost::optional<WalletAddress> getAddress(const WalletID& id) const override
        {
            if (id == m_LastAdddr.m_walletID)
                return m_LastAdddr;

            return boost::optional<WalletAddress>();
        }
        void deleteAddress(const WalletID&) override {}

        Height getCurrentHeight() const override
        {
            return 134;
        }

        void rollbackConfirmedUtxo(Height /*minHeight*/) override
        {}

        void clear() override {}

        void changePassword(const SecString& password) override {}

        bool setTxParameter(const TxID& txID, wallet::TxParameterID paramID,
            const ByteBuffer& blob, bool shouldNotifyAboutChanges) override
        {
            if (paramID < wallet::TxParameterID::PrivateFirstParam)
            {
                auto p = m_params.emplace(paramID, blob);
                return p.second;
            }
            m_params[paramID] = blob;
            return true;
        }
        bool getTxParameter(const TxID& txID, wallet::TxParameterID paramID, ByteBuffer& blob) const override
        {
            auto it = m_params.find(paramID);
            if (it != m_params.end())
            {
                blob = it->second;
                return true;
            }
            return false;
        }

        Block::SystemState::IHistory& get_History() override { return m_Hist; }
        void ShrinkHistory() override {}

        Amount getTransferredByTx(TxStatus status, bool isSender) const override { return 0; };

    protected:
        std::vector<beam::Coin> m_coins;
        std::map<wallet::TxParameterID, ByteBuffer> m_params;
    };

    class TestWalletDB : public BaseTestWalletDB
    {
    public:
        TestWalletDB()
        {
            m_coins.emplace_back(5);
            m_coins.emplace_back(2);
            m_coins.emplace_back(3);
        }
    };

    class TestWalletDB2 : public BaseTestWalletDB
    {
    public:
        TestWalletDB2()
        {
            m_coins.emplace_back(1);
            m_coins.emplace_back(3);
        }
    };

    template<typename T>
    IWalletDB::Ptr CreateWalletDB()
    {
        return std::static_pointer_cast<IWalletDB>(std::make_shared<T>());
    }

    const string SenderWalletDB = "sender_wallet.db";
    const string ReceiverWalletDB = "receiver_wallet.db";
    const string DBPassword = "pass123";

    IWalletDB::Ptr createSqliteWalletDB(const string& path, bool separateDBForPrivateData)
    {
        if (boost::filesystem::exists(path))
        {
            boost::filesystem::remove(path);
        }
        if (separateDBForPrivateData)
        {
            string privatePath = path + ".private";
            boost::filesystem::remove(privatePath);
        }
        
        ECC::NoLeak<ECC::uintBig> seed;
        seed.V = Zero;
               
        auto walletDB = WalletDB::init(path, DBPassword, seed, io::Reactor::get_Current().shared_from_this(), separateDBForPrivateData);
        return walletDB;
    }

    IWalletDB::Ptr createSenderWalletDB(bool separateDBForPrivateData = false)
    {
        auto db = createSqliteWalletDB(SenderWalletDB, separateDBForPrivateData);
        db->AllocateKidRange(100500); // make sure it'll get the address different from the receiver
        for (auto amount : { 5, 2, 1, 9 })
        {
            Coin coin = CreateAvailCoin(amount, 0);
            db->store(coin);
        }
        return db;
    }

    IWalletDB::Ptr createSenderWalletDB(int count, Amount amount, bool separateDBForPrivateData = false)
    {
        auto db = createSqliteWalletDB(SenderWalletDB, separateDBForPrivateData);
        db->AllocateKidRange(100500); // make sure it'll get the address different from the receiver
        for (int i = 0; i < count; ++i)
        {
            Coin coin = CreateAvailCoin(amount, 0);
            db->store(coin);
        }
        return db;
    }

    IWalletDB::Ptr createReceiverWalletDB(bool separateDBForPrivateData = false)
    {
        return createSqliteWalletDB(ReceiverWalletDB, separateDBForPrivateData);
    }

    struct TestGateway : wallet::INegotiatorGateway
    {
        void on_tx_completed(const TxID&) override
        {
            cout << __FUNCTION__ << "\n";
        }

        void register_tx(const TxID& , Transaction::Ptr) override
        {
            cout << "sent tx registration request\n";
        }

        void confirm_outputs(const vector<Coin>&) override
        {
            cout << "confirm outputs\n";
        }

        void confirm_kernel(const TxID&, const Merkle::Hash&) override
        {
            cout << "confirm kernel\n";
        }

        bool get_tip(Block::SystemState::Full& state) const override
        {
            return true;
        }

    };

    class AsyncProcessor
    {
        io::Timer::Ptr m_pTimer;
        bool m_bPending = false;

    public:
        virtual void Proceed() = 0;

        void PostAsync()
        {
            if (!m_bPending)
            {
                if (!m_pTimer)
                    m_pTimer = io::Timer::create(io::Reactor::get_Current());

                m_bPending = true;
                m_pTimer->start(0, false, [this]() {
                    assert(m_bPending);
                    m_bPending = false;
                    Proceed();
                });
            }
        }
    };

    class OneTimeBbsEndpoint : public WalletNetworkViaBbs
    {
    public:
        OneTimeBbsEndpoint(IWallet& wallet, std::shared_ptr<proto::FlyClient::INetwork> nodeEndpoint, const IWalletDB::Ptr& walletDB)
            : WalletNetworkViaBbs(wallet, nodeEndpoint, walletDB)
        {

        }
    private:
        void OnIncomingMessage() override
        {
            io::Reactor::get_Current().stop();
        }

    };
        

    struct TestWalletRig
    {
        TestWalletRig(const string& name, IWalletDB::Ptr walletDB, Wallet::TxCompletedAction&& action = Wallet::TxCompletedAction(), bool coldWallet = false, bool oneTimeBbsEndpoint = false)
            : m_WalletDB{walletDB}
            , m_Wallet{ m_WalletDB, move(action), coldWallet ? []() {io::Reactor::get_Current().stop(); } : Wallet::UpdateCompletedAction() }
        {
            if (m_WalletDB->get_MasterKdf()) // can create secrets
            {
                WalletAddress wa = wallet::createAddress(*m_WalletDB);
                m_WalletDB->saveAddress(wa);
                m_WalletID = wa.m_walletID;
            }
            else
            {
                auto addresses = m_WalletDB->getAddresses(true);
                m_WalletID = addresses[0].m_walletID;
            }

            if (coldWallet)
            {
                m_Wallet.AddMessageEndpoint(make_shared<ColdWalletMessageEndpoint>(m_Wallet, m_WalletDB));
            }
            else
            {
                auto nodeEndpoint = make_shared<proto::FlyClient::NetworkStd>(m_Wallet);
                nodeEndpoint->m_Cfg.m_vNodes.push_back(io::Address::localhost().port(32125));
                nodeEndpoint->Connect();
                if (oneTimeBbsEndpoint)
                {
                    m_Wallet.AddMessageEndpoint(make_shared<OneTimeBbsEndpoint>(m_Wallet, nodeEndpoint, m_WalletDB));
                }
                else
                {
                    m_Wallet.AddMessageEndpoint(make_shared<WalletNetworkViaBbs>(m_Wallet, nodeEndpoint, m_WalletDB));
                }
                m_Wallet.SetNodeEndpoint(nodeEndpoint);
            }
        }

        vector<Coin> GetCoins()
        {
            vector<Coin> coins;
            m_WalletDB->visit([&coins](const Coin& c)->bool
            {
                coins.push_back(c);
                return true;
            });
            return coins;
        }

        WalletID m_WalletID;
        IWalletDB::Ptr m_WalletDB;
        int m_CompletedCount{1};
        Wallet m_Wallet;
    };

    struct TestWalletNetwork
        : public IWalletMessageEndpoint
        , public AsyncProcessor
    {
        struct Entry
        {
            IWallet* m_pSink;
            std::deque<std::pair<WalletID, wallet::SetTxParameter> > m_Msgs;
        };

        typedef std::map<WalletID, Entry> WalletMap;
        WalletMap m_Map;

        virtual void Send(const WalletID& peerID, const wallet::SetTxParameter& msg) override
        {
            WalletMap::iterator it = m_Map.find(peerID);
            WALLET_CHECK(m_Map.end() != it);

            it->second.m_Msgs.push_back(std::make_pair(peerID, msg));

            PostAsync();
        }

        virtual void SendEncryptedMessage(const WalletID& peerID, const ByteBuffer& msg) override
        {
        }

        virtual void Proceed() override
        {
            for (WalletMap::iterator it = m_Map.begin(); m_Map.end() != it; it++)
                for (Entry& v = it->second; !v.m_Msgs.empty(); v.m_Msgs.pop_front())
                    v.m_pSink->OnWalletMessage(v.m_Msgs.front().first, std::move(v.m_Msgs.front().second));
        }
    };

    struct TestBlockchain
    {
        MiniBlockChain m_mcm;

        UtxoTree m_Utxos;

        struct KrnPerBlock
        {
            std::vector<Merkle::Hash> m_vKrnIDs;

            struct Mmr :public Merkle::FlyMmr
            {
                const Merkle::Hash* m_pHashes;

                Mmr(const KrnPerBlock& kpb)
                    :Merkle::FlyMmr(kpb.m_vKrnIDs.size())
                {
                    m_pHashes = kpb.m_vKrnIDs.empty() ? NULL : &kpb.m_vKrnIDs.front();
                }

                virtual void LoadElement(Merkle::Hash& hv, uint64_t n) const override {
                    hv = m_pHashes[n];
                }
            };

        };
        std::vector<KrnPerBlock> m_vBlockKernels;

        void AddBlock()
        {
            m_Utxos.get_Hash(m_mcm.m_hvLive);
            m_mcm.Add();

            if (m_vBlockKernels.size() < m_mcm.m_vStates.size())
                m_vBlockKernels.emplace_back();
            assert(m_vBlockKernels.size() == m_mcm.m_vStates.size());

            KrnPerBlock::Mmr fmmr(m_vBlockKernels.back());
            fmmr.get_Hash(m_mcm.m_vStates.back().m_Hdr.m_Kernels);
        }

        bool AddCommitment(const ECC::Point& c)
        {
            UtxoTree::Key::Data d;
            d.m_Commitment = c;
            d.m_Maturity = m_mcm.m_vStates.back().m_Hdr.m_Height;

            UtxoTree::Key key;
            key = d;

            UtxoTree::Cursor cu;
            bool bCreate = true;
            UtxoTree::MyLeaf* p = m_Utxos.Find(cu, key, bCreate);

            cu.InvalidateElement();

            if (bCreate)
                p->m_ID = 0;
            else
            {
                // protect again overflow attacks, though it's highly unlikely (Input::Count is currently limited to 32 bits, it'd take millions of blocks)
                Input::Count nCountInc = p->get_Count() + 1;
                if (!nCountInc)
                    return false;

                p->PushID(0);
            }

            return true;
        }

        bool RemoveCommitment(const ECC::Point& c)
        {
            UtxoTree::Cursor cu;
            UtxoTree::MyLeaf* p;
            UtxoTree::Key::Data d;
            d.m_Commitment = c;

            struct Traveler :public UtxoTree::ITraveler {
                virtual bool OnLeaf(const RadixTree::Leaf& x) override {
                    return false; // stop iteration
                }
            } t;


            UtxoTree::Key kMin, kMax;

            d.m_Maturity = 0;
            kMin = d;
            d.m_Maturity = m_mcm.m_vStates.back().m_Hdr.m_Height;
            kMax = d;

            t.m_pCu = &cu;
            t.m_pBound[0] = kMin.m_pArr;
            t.m_pBound[1] = kMax.m_pArr;

            if (m_Utxos.Traverse(t))
                return false;

            p = &(UtxoTree::MyLeaf&) cu.get_Leaf();

            d = p->m_Key;
            assert(d.m_Commitment == c);

            if (!p->IsExt())
                m_Utxos.Delete(cu);
            else
            {
                p->PopID();
                cu.InvalidateElement();
            }

            return true;
        }


        void GetProof(const proto::GetProofUtxo& data, proto::ProofUtxo& msgOut)
        {
            struct Traveler :public UtxoTree::ITraveler
            {
                proto::ProofUtxo m_Msg;
                UtxoTree* m_pTree;
                Merkle::Hash m_hvHistory;

                virtual bool OnLeaf(const RadixTree::Leaf& x) override {

                    const UtxoTree::MyLeaf& v = (UtxoTree::MyLeaf&) x;
                    UtxoTree::Key::Data d;
                    d = v.m_Key;

                    m_Msg.m_Proofs.resize(m_Msg.m_Proofs.size() + 1);
                    Input::Proof& ret = m_Msg.m_Proofs.back();

                    ret.m_State.m_Count = v.get_Count();
                    ret.m_State.m_Maturity = d.m_Maturity;
                    m_pTree->get_Proof(ret.m_Proof, *m_pCu);

                    ret.m_Proof.emplace_back();
                    ret.m_Proof.back().first = false;
                    ret.m_Proof.back().second = m_hvHistory;

                    return m_Msg.m_Proofs.size() < Input::Proof::s_EntriesMax;
                }
            } t;

            t.m_pTree = &m_Utxos;
            m_mcm.m_Mmr.get_Hash(t.m_hvHistory);

            UtxoTree::Cursor cu;
            t.m_pCu = &cu;

            // bounds
            UtxoTree::Key kMin, kMax;

            UtxoTree::Key::Data d;
            d.m_Commitment = data.m_Utxo;
            d.m_Maturity = data.m_MaturityMin;
            kMin = d;
            d.m_Maturity = Height(-1);
            kMax = d;

            t.m_pBound[0] = kMin.m_pArr;
            t.m_pBound[1] = kMax.m_pArr;

            t.m_pTree->Traverse(t);
            t.m_Msg.m_Proofs.swap(msgOut.m_Proofs);
        }

        void GetProof(const proto::GetProofKernel& data, proto::ProofKernel& msgOut)
        {
            for (size_t iState = m_mcm.m_vStates.size(); iState--; )
            {
                const KrnPerBlock& kpb = m_vBlockKernels[iState];

                for (size_t i = 0; i < kpb.m_vKrnIDs.size(); i++)
                {
                    if (kpb.m_vKrnIDs[i] == data.m_ID)
                    {
                        KrnPerBlock::Mmr fmmr(kpb);
                        Merkle::ProofBuilderStd bld;
                        fmmr.get_Proof(bld, i);

                        msgOut.m_Proof.m_Inner.swap(bld.m_Proof);
                        msgOut.m_Proof.m_State = m_mcm.m_vStates[iState].m_Hdr;

                        if (iState + 1 != m_mcm.m_vStates.size())
                        {
                            Merkle::ProofBuilderHard bld2;
                            m_mcm.m_Mmr.get_Proof(bld2, iState);
                            msgOut.m_Proof.m_Outer.swap(bld2.m_Proof);
                            msgOut.m_Proof.m_Outer.resize(msgOut.m_Proof.m_Outer.size() + 1);
                            m_Utxos.get_Hash(msgOut.m_Proof.m_Outer.back());

                            Block::SystemState::Full state = m_mcm.m_vStates[m_mcm.m_vStates.size()-1].m_Hdr;
                            WALLET_CHECK(state.IsValidProofKernel(data.m_ID, msgOut.m_Proof));
                        }

                        return;
                    }
                }
            }
        }

        void AddKernel(const TxKernel& krn)
        {
            Merkle::Hash hvKrn;
            krn.get_Hash(hvKrn);
            AddKernel(hvKrn);
        }

        void AddKernel(const Merkle::Hash& hvKrn)
        {
            if (m_vBlockKernels.size() <= m_mcm.m_vStates.size())
                m_vBlockKernels.emplace_back();

            KrnPerBlock& kpb = m_vBlockKernels.back();
            kpb.m_vKrnIDs.push_back(hvKrn);
        }

        void HandleTx(const proto::NewTransaction& data)
        {
            for (const auto& input : data.m_Transaction->m_vInputs)
                RemoveCommitment(input->m_Commitment);
            for (const auto& output : data.m_Transaction->m_vOutputs)
                AddCommitment(output->m_Commitment);
            for (size_t i = 0; i < data.m_Transaction->m_vKernels.size(); i++)
                AddKernel(*data.m_Transaction->m_vKernels[i]);
        }
    };

    struct TestNodeNetwork
        :public proto::FlyClient::INetwork
        ,public AsyncProcessor
        ,public boost::intrusive::list_base_hook<>
    {
        typedef boost::intrusive::list<TestNodeNetwork> List;
        typedef proto::FlyClient::Request Request;

        proto::FlyClient& m_Client;

        struct Shared
        {
            TestBlockchain m_Blockchain;
            List m_lst;

            void AddBlock()
            {
                m_Blockchain.AddBlock();

                for (List::iterator it = m_lst.begin(); m_lst.end() != it; it++)
                {
                    proto::FlyClient& c = it->m_Client;
                    c.get_History().AddStates(&m_Blockchain.m_mcm.m_vStates.back().m_Hdr, 1);
                    c.OnNewTip();
                }
            }
        };

        Shared& m_Shared;

        TestNodeNetwork(Shared& shared, proto::FlyClient& x)
            :m_Client(x)
            ,m_Shared(shared)
        {
            m_Shared.m_lst.push_back(*this);
        }

        ~TestNodeNetwork()
        {
            m_Shared.m_lst.erase(List::s_iterator_to(*this));
        }

        typedef std::deque<Request::Ptr> Queue;
        Queue m_queReqs;

        virtual void Connect() override {}
        virtual void Disconnect() override {}

        virtual void PostRequestInternal(Request& r) override
        {
            assert(r.m_pTrg);

            m_queReqs.push_back(&r);
            PostAsync();
        }

        virtual void Proceed() override
        {
            Queue q;
            q.swap(m_queReqs);

            for (; !q.empty(); q.pop_front())
            {
                Request& r = *q.front();
                PostProcess(r);
                if (r.m_pTrg)
                    r.m_pTrg->OnComplete(r);
            }
        }

        virtual void PostProcess(Request& r)
        {
            switch (r.get_Type())
            {
            case Request::Type::Transaction:
                {
                    proto::FlyClient::RequestTransaction& v = static_cast<proto::FlyClient::RequestTransaction&>(r);
                    v.m_Res.m_Value = true;

                    m_Shared.m_Blockchain.HandleTx(v.m_Msg);
                    m_Shared.AddBlock();
                }
                break;

            case Request::Type::Kernel:
                {
                    proto::FlyClient::RequestKernel& v = static_cast<proto::FlyClient::RequestKernel&>(r);
                    m_Shared.m_Blockchain.GetProof(v.m_Msg, v.m_Res);
                }
                break;

            case Request::Type::Utxo:
                {
                    proto::FlyClient::RequestUtxo& v = static_cast<proto::FlyClient::RequestUtxo&>(r);
                    m_Shared.m_Blockchain.GetProof(v.m_Msg, v.m_Res);
                }
                break;

            default:
                break; // suppess warning
            }
        }
    };

    void TestWalletNegotiation(IWalletDB::Ptr senderWalletDB, IWalletDB::Ptr receiverWalletDB)
    {
        cout << "\nTesting wallets negotiation...\n";

        io::Reactor::Ptr mainReactor{ io::Reactor::create() };
        io::Reactor::Scope scope(*mainReactor);

        WalletAddress wa = wallet::createAddress(*receiverWalletDB);
        receiverWalletDB->saveAddress(wa);
        WalletID receiver_id = wa.m_walletID;

        wa = wallet::createAddress(*senderWalletDB);
        senderWalletDB->saveAddress(wa);
        WalletID sender_id = wa.m_walletID;

        int count = 0;
        auto f = [&count](const auto& /*id*/)
        {
            if (++count >= 2)
                io::Reactor::get_Current().stop();
        };
        
        TestNodeNetwork::Shared tnns;

        Wallet sender(senderWalletDB, f);
        Wallet receiver(receiverWalletDB, f);

        auto twn = make_shared<TestWalletNetwork>();
        auto netNodeS = make_shared<TestNodeNetwork>(tnns, sender);
        auto netNodeR = make_shared<TestNodeNetwork>(tnns, receiver);

        sender.AddMessageEndpoint(twn);
        sender.SetNodeEndpoint(netNodeS);

        receiver.AddMessageEndpoint(twn);
        receiver.SetNodeEndpoint(netNodeR);

        twn->m_Map[sender_id].m_pSink = &sender;
        twn->m_Map[receiver_id].m_pSink = &receiver;

        tnns.AddBlock();

        sender.transfer_money(sender_id, receiver_id, 6, 1, true, 200, {});
        mainReactor->run();

        WALLET_CHECK(count == 2);
    }

    class TestNode
    {
    public:
        TestNode()
        {
            m_Server.Listen(io::Address::localhost().port(32125));
            while (m_Blockchain.m_mcm.m_vStates.size() < 145)
                m_Blockchain.AddBlock();
        }

        ~TestNode() {
            KillAll();
        }

        void KillAll()
        {
            while (!m_lstClients.empty())
                DeleteClient(&m_lstClients.front());
        }

        TestBlockchain m_Blockchain;

        void AddBlock()
        {
            m_Blockchain.AddBlock();

            for (ClientList::iterator it = m_lstClients.begin(); m_lstClients.end() != it; it++)
            {
                Client& c = *it;
                if (c.IsSecureOut())
                    c.SendTip();
            }
        }
    private:

        struct Client
            :public proto::NodeConnection
            ,public boost::intrusive::list_base_hook<>
        {
            TestNode& m_This;
            bool m_Subscribed;

            Client(TestNode& n)
                : m_This(n)
                , m_Subscribed(false)
            {
            }


            // protocol handler
            void OnConnectedSecure() override
            {
                ECC::Scalar::Native sk;
                sk = 23U;
                ProveID(sk, proto::IDType::Node);

                proto::Login msg;
                msg.m_CfgChecksum = Rules::get().Checksum;
                msg.m_Flags =
                    proto::LoginFlags::Extension1 |
                    proto::LoginFlags::SpreadingTransactions |
                    proto::LoginFlags::Bbs |
                    proto::LoginFlags::SendPeers;
                Send(msg);

                SendTip();
            }

            void SendTip()
            {
                proto::NewTip msg;
                msg.m_Description = m_This.m_Blockchain.m_mcm.m_vStates.back().m_Hdr;
                Send(msg);
            }

            void OnMsg(proto::NewTransaction&& data) override
            {
                m_This.m_Blockchain.HandleTx(data);

                Send(proto::Boolean{ true });
                m_This.AddBlock();
            }

            void OnMsg(proto::GetProofUtxo&& data) override
            {
                proto::ProofUtxo msgOut;
                m_This.m_Blockchain.GetProof(data, msgOut);
                Send(msgOut);
            }

            void OnMsg(proto::GetProofKernel&& data) override
            {
                proto::ProofKernel msgOut;
                m_This.m_Blockchain.GetProof(data, msgOut);
                Send(msgOut);
            }

            void OnMsg(proto::Login&& /*data*/) override
            {
            }

            void OnMsg(proto::GetProofState&&) override
            {
                Send(proto::ProofState{});
            }

            void OnMsg(proto::GetProofChainWork&& msg) override
            {
                proto::ProofChainWork msgOut;
                msgOut.m_Proof.m_LowerBound = msg.m_LowerBound;
                msgOut.m_Proof.m_hvRootLive = m_This.m_Blockchain.m_mcm.m_hvLive;
                msgOut.m_Proof.Create(m_This.m_Blockchain.m_mcm.m_Source, m_This.m_Blockchain.m_mcm.m_vStates.back().m_Hdr);

                Send(msgOut);
            }

            void OnMsg(proto::BbsSubscribe&& msg) override
            {
                if (m_Subscribed)
                    return;
                m_Subscribed = true;

                for (const auto& m : m_This.m_bbs)
                    Send(m);
            }

            void OnMsg(proto::BbsMsg&& msg) override
            {
                m_This.m_bbs.push_back(msg);

                for (ClientList::iterator it = m_This.m_lstClients.begin(); m_This.m_lstClients.end() != it; it++)
                {
                    Client& c = *it;
                    if ((&c != this) && c.m_Subscribed)
                        c.Send(msg);
                }
            }

            void OnMsg(proto::Ping&& msg) override
            {
                proto::Pong msgOut(Zero);
                Send(msgOut);
            }

            void OnDisconnect(const DisconnectReason& r) override
            {
                switch (r.m_Type)
                {
                case DisconnectReason::Protocol:
                case DisconnectReason::ProcessingExc:
                    LOG_ERROR() << "Disconnect: " << r;
                    g_failureCount++;

                default: // suppress warning
                    break;
                }

                m_This.DeleteClient(this);
            }
        };

        typedef boost::intrusive::list<Client> ClientList;
        ClientList m_lstClients;

        std::vector<proto::BbsMsg> m_bbs;

        void DeleteClient(Client* client)
        {
            m_lstClients.erase(ClientList::s_iterator_to(*client));
            delete client;
        }

        struct Server
            :public proto::NodeConnection::Server
        {
            IMPLEMENT_GET_PARENT_OBJ(TestNode, m_Server)

            void OnAccepted(io::TcpStream::Ptr&& newStream, int errorCode) override
            {
                if (newStream)
                {
                    Client* p = new Client(get_ParentObj());
                    get_ParentObj().m_lstClients.push_back(*p);

                    p->Accept(std::move(newStream));
                    p->SecureConnect();
                }
            }
        } m_Server;
    };

    void TestTxToHimself()
    {
        cout << "\nTesting Tx to himself...\n";

        io::Reactor::Ptr mainReactor{ io::Reactor::create() };
        io::Reactor::Scope scope(*mainReactor);

        auto senderWalletDB = createSqliteWalletDB("sender_wallet.db", false);

        // add coin with keyType - Coinbase
        beam::Amount coin_amount = 40;
        Coin coin = CreateAvailCoin(coin_amount, 0);
        coin.m_ID.m_Type = Key::Type::Coinbase;
        senderWalletDB->store(coin);

        auto coins = senderWalletDB->selectCoins(24);
        WALLET_CHECK(coins.size() == 1);
        WALLET_CHECK(coins[0].m_ID.m_Type == Key::Type::Coinbase);
        WALLET_CHECK(coins[0].m_status == Coin::Available);
        WALLET_CHECK(senderWalletDB->getTxHistory().empty());

        TestNode node;
        TestWalletRig sender("sender", senderWalletDB, [](auto) { io::Reactor::get_Current().stop(); });
        helpers::StopWatch sw;

        sw.start();
        auto txId = sender.m_Wallet.transfer_money(sender.m_WalletID, sender.m_WalletID, 24, 2, true, 200);
        mainReactor->run();
        sw.stop();

        cout << "Transfer elapsed time: " << sw.milliseconds() << " ms\n";

        // check Tx
        auto txHistory = senderWalletDB->getTxHistory();
        WALLET_CHECK(txHistory.size() == 1);
        WALLET_CHECK(txHistory[0].m_txId == txId);
        WALLET_CHECK(txHistory[0].m_amount == 24);
        WALLET_CHECK(txHistory[0].m_change == 14);
        WALLET_CHECK(txHistory[0].m_fee == 2);
        WALLET_CHECK(txHistory[0].m_status == TxStatus::Completed);

        // check coins
        vector<Coin> newSenderCoins;
        senderWalletDB->visit([&newSenderCoins](const Coin& c)->bool
        {
            newSenderCoins.push_back(c);
            return true;
        });

        WALLET_CHECK(newSenderCoins.size() == 3);

        WALLET_CHECK(newSenderCoins[0].m_ID.m_Type == Key::Type::Coinbase);
        WALLET_CHECK(newSenderCoins[0].m_status == Coin::Spent);
        WALLET_CHECK(newSenderCoins[0].m_ID.m_Value == 40);

        WALLET_CHECK(newSenderCoins[1].m_ID.m_Type == Key::Type::Change);
        WALLET_CHECK(newSenderCoins[1].m_status == Coin::Available);
        WALLET_CHECK(newSenderCoins[1].m_ID.m_Value == 14);

        WALLET_CHECK(newSenderCoins[2].m_ID.m_Type == Key::Type::Regular);
        WALLET_CHECK(newSenderCoins[2].m_status == Coin::Available);
        WALLET_CHECK(newSenderCoins[2].m_ID.m_Value == 24);

        cout << "\nFinish of testing Tx to himself...\n";
    }

    void TestP2PWalletNegotiationST()
    {
        cout << "\nTesting p2p wallets negotiation single thread...\n";

        io::Reactor::Ptr mainReactor{ io::Reactor::create() };
        io::Reactor::Scope scope(*mainReactor);

        int completedCount = 2;
        auto f = [&completedCount, mainReactor](auto)
        {
            --completedCount;
            if (completedCount == 0)
            {
                mainReactor->stop();
                completedCount = 2;
            }
        };

        TestNode node;
        TestWalletRig sender("sender", createSenderWalletDB(), f);
        TestWalletRig receiver("receiver", createReceiverWalletDB(), f);

        WALLET_CHECK(sender.m_WalletDB->selectCoins(6).size() == 2);
        WALLET_CHECK(sender.m_WalletDB->getTxHistory().empty());
        WALLET_CHECK(receiver.m_WalletDB->getTxHistory().empty());

        helpers::StopWatch sw;
        sw.start();

        auto txId = sender.m_Wallet.transfer_money(sender.m_WalletID, receiver.m_WalletID, 4, 2, true, 200);

        mainReactor->run();
        sw.stop();
        cout << "First transfer elapsed time: " << sw.milliseconds() << " ms\n";

        // check coins
        vector<Coin> newSenderCoins = sender.GetCoins();
        vector<Coin> newReceiverCoins = receiver.GetCoins();

        WALLET_CHECK(newSenderCoins.size() == 4);
        WALLET_CHECK(newReceiverCoins.size() == 1);
        WALLET_CHECK(newReceiverCoins[0].m_ID.m_Value == 4);
        WALLET_CHECK(newReceiverCoins[0].m_status == Coin::Available);
        WALLET_CHECK(newReceiverCoins[0].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[0].m_ID.m_Value == 5);
        WALLET_CHECK(newSenderCoins[0].m_status == Coin::Spent);
        WALLET_CHECK(newSenderCoins[0].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[1].m_ID.m_Value == 2);
        WALLET_CHECK(newSenderCoins[1].m_status == Coin::Available);
        WALLET_CHECK(newSenderCoins[1].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[2].m_ID.m_Value == 1);
        WALLET_CHECK(newSenderCoins[2].m_status == Coin::Spent);
        WALLET_CHECK(newSenderCoins[2].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[3].m_ID.m_Value == 9);
        WALLET_CHECK(newSenderCoins[3].m_status == Coin::Available);
        WALLET_CHECK(newSenderCoins[3].m_ID.m_Type == Key::Type::Regular);

        // Tx history check
        auto sh = sender.m_WalletDB->getTxHistory();
        WALLET_CHECK(sh.size() == 1);
        auto rh = receiver.m_WalletDB->getTxHistory();
        WALLET_CHECK(rh.size() == 1);
        auto stx = sender.m_WalletDB->getTx(txId);
        WALLET_CHECK(stx.is_initialized());
        auto rtx = receiver.m_WalletDB->getTx(txId);
        WALLET_CHECK(rtx.is_initialized());

        WALLET_CHECK(stx->m_txId == rtx->m_txId);
        WALLET_CHECK(stx->m_amount == rtx->m_amount);
        WALLET_CHECK(stx->m_status == TxStatus::Completed);
        WALLET_CHECK(stx->m_fee == rtx->m_fee);
        WALLET_CHECK(stx->m_message == rtx->m_message);
        WALLET_CHECK(stx->m_createTime <= rtx->m_createTime);
        WALLET_CHECK(stx->m_status == rtx->m_status);
        WALLET_CHECK(stx->m_sender == true);
        WALLET_CHECK(rtx->m_sender == false);

        // second transfer
        auto preselectedCoins = sender.m_WalletDB->selectCoins(6);
        CoinIDList preselectedIDs;
        for (const auto& c : preselectedCoins)
        {
            preselectedIDs.push_back(c.m_ID);
        }
        sw.start();
        txId = sender.m_Wallet.transfer_money(sender.m_WalletID, receiver.m_WalletID, 6, 0, preselectedIDs, true, 200);
        mainReactor->run();
        sw.stop();
        cout << "Second transfer elapsed time: " << sw.milliseconds() << " ms\n";

        // check coins
        newSenderCoins = sender.GetCoins();
        newReceiverCoins = receiver.GetCoins();

        WALLET_CHECK(newSenderCoins.size() == 5);
        WALLET_CHECK(newReceiverCoins.size() == 2);

        WALLET_CHECK(newReceiverCoins[0].m_ID.m_Value == 4);
        WALLET_CHECK(newReceiverCoins[0].m_status == Coin::Available);
        WALLET_CHECK(newReceiverCoins[0].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newReceiverCoins[1].m_ID.m_Value == 6);
        WALLET_CHECK(newReceiverCoins[1].m_status == Coin::Available);
        WALLET_CHECK(newReceiverCoins[1].m_ID.m_Type == Key::Type::Regular);


        WALLET_CHECK(newSenderCoins[0].m_ID.m_Value == 5);
        WALLET_CHECK(newSenderCoins[0].m_status == Coin::Spent);
        WALLET_CHECK(newSenderCoins[0].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[1].m_ID.m_Value == 2);
        WALLET_CHECK(newSenderCoins[1].m_status == Coin::Available);
        WALLET_CHECK(newSenderCoins[1].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[2].m_ID.m_Value == 1);
        WALLET_CHECK(newSenderCoins[2].m_status == Coin::Spent);
        WALLET_CHECK(newSenderCoins[2].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[3].m_ID.m_Value == 9);
        WALLET_CHECK(newSenderCoins[3].m_status == Coin::Spent);
        WALLET_CHECK(newSenderCoins[3].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[4].m_ID.m_Value == 3);
        WALLET_CHECK(newSenderCoins[4].m_status == Coin::Available);
        WALLET_CHECK(newSenderCoins[4].m_ID.m_Type == Key::Type::Change);

        // Tx history check
        sh = sender.m_WalletDB->getTxHistory();
        WALLET_CHECK(sh.size() == 2);
        rh = receiver.m_WalletDB->getTxHistory();
        WALLET_CHECK(rh.size() == 2);
        stx = sender.m_WalletDB->getTx(txId);
        WALLET_CHECK(stx.is_initialized());
        rtx = receiver.m_WalletDB->getTx(txId);
        WALLET_CHECK(rtx.is_initialized());

        WALLET_CHECK(stx->m_txId == rtx->m_txId);
        WALLET_CHECK(stx->m_amount == rtx->m_amount);
        WALLET_CHECK(stx->m_status == TxStatus::Completed);
        WALLET_CHECK(stx->m_message == rtx->m_message);
        WALLET_CHECK(stx->m_createTime <= rtx->m_createTime);
        WALLET_CHECK(stx->m_status == rtx->m_status);
        WALLET_CHECK(stx->m_sender == true);
        WALLET_CHECK(rtx->m_sender == false);


        // third transfer. no enough money should appear
        sw.start();
        completedCount = 1;// only one wallet takes part in tx
        txId = sender.m_Wallet.transfer_money(sender.m_WalletID, receiver.m_WalletID, 6, 0, true, 200);
        mainReactor->run();
        sw.stop();
        cout << "Third transfer elapsed time: " << sw.milliseconds() << " ms\n";
    
        // check coins
        newSenderCoins = sender.GetCoins();
        newReceiverCoins = receiver.GetCoins();

        // no coins 
        WALLET_CHECK(newSenderCoins.size() == 5);
        WALLET_CHECK(newReceiverCoins.size() == 2);

        // Tx history check. New failed tx should be added to sender
        sh = sender.m_WalletDB->getTxHistory();
        WALLET_CHECK(sh.size() == 3);
        rh = receiver.m_WalletDB->getTxHistory();
        WALLET_CHECK(rh.size() == 2);
        stx = sender.m_WalletDB->getTx(txId);
        WALLET_CHECK(stx.is_initialized());
        rtx = receiver.m_WalletDB->getTx(txId);
        WALLET_CHECK(!rtx.is_initialized());

        WALLET_CHECK(stx->m_amount == 6);
        WALLET_CHECK(stx->m_status == TxStatus::Failed);
        WALLET_CHECK(stx->m_sender == true);
        WALLET_CHECK(stx->m_failureReason == TxFailureReason::NoInputs);
    }

    void TestP2PWalletReverseNegotiationST()
    {
        cout << "\nTesting p2p wallets negotiation (reverse version)...\n";

        io::Reactor::Ptr mainReactor{ io::Reactor::create() };
        io::Reactor::Scope scope(*mainReactor);

        int completedCount = 2;
        auto f = [&completedCount, mainReactor](auto)
        {
            --completedCount;
            if (completedCount == 0)
            {
                mainReactor->stop();
                completedCount = 2;
            }
        };

        TestNode node;
        TestWalletRig sender("sender", createSenderWalletDB(), f);
        TestWalletRig receiver("receiver", createReceiverWalletDB(), f);
  
        WALLET_CHECK(sender.m_WalletDB->selectCoins(6).size() == 2);
        WALLET_CHECK(sender.m_WalletDB->getTxHistory().empty());
        WALLET_CHECK(receiver.m_WalletDB->getTxHistory().empty());

        helpers::StopWatch sw;
        sw.start();

        auto txId = receiver.m_Wallet.transfer_money(receiver.m_WalletID, sender.m_WalletID, 4, 2, false, 200);

        mainReactor->run();
        sw.stop();
        cout << "First transfer elapsed time: " << sw.milliseconds() << " ms\n";

        // check coins
        vector<Coin> newSenderCoins = sender.GetCoins();
        vector<Coin> newReceiverCoins = receiver.GetCoins();

        WALLET_CHECK(newSenderCoins.size() == 4);
        WALLET_CHECK(newReceiverCoins.size() == 1);
        WALLET_CHECK(newReceiverCoins[0].m_ID.m_Value == 4);
        WALLET_CHECK(newReceiverCoins[0].m_status == Coin::Available);
        WALLET_CHECK(newReceiverCoins[0].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[0].m_ID.m_Value == 5);
        WALLET_CHECK(newSenderCoins[0].m_status == Coin::Spent);
        WALLET_CHECK(newSenderCoins[0].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[1].m_ID.m_Value == 2);
        WALLET_CHECK(newSenderCoins[1].m_status == Coin::Available);
        WALLET_CHECK(newSenderCoins[1].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[2].m_ID.m_Value == 1);
        WALLET_CHECK(newSenderCoins[2].m_status == Coin::Spent);
        WALLET_CHECK(newSenderCoins[2].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[3].m_ID.m_Value == 9);
        WALLET_CHECK(newSenderCoins[3].m_status == Coin::Available);
        WALLET_CHECK(newSenderCoins[3].m_ID.m_Type == Key::Type::Regular);

        // Tx history check
        auto sh = sender.m_WalletDB->getTxHistory();
        WALLET_CHECK(sh.size() == 1);
        auto rh = receiver.m_WalletDB->getTxHistory();
        WALLET_CHECK(rh.size() == 1);
        auto stx = sender.m_WalletDB->getTx(txId);
        WALLET_CHECK(stx.is_initialized());
        auto rtx = receiver.m_WalletDB->getTx(txId);
        WALLET_CHECK(rtx.is_initialized());

        WALLET_CHECK(stx->m_txId == rtx->m_txId);
        WALLET_CHECK(stx->m_amount == rtx->m_amount);
        WALLET_CHECK(stx->m_status == TxStatus::Completed);
        WALLET_CHECK(stx->m_fee == rtx->m_fee);
        WALLET_CHECK(stx->m_message == rtx->m_message);
        WALLET_CHECK(stx->m_createTime >= rtx->m_createTime);
        WALLET_CHECK(stx->m_status == rtx->m_status);
        WALLET_CHECK(stx->m_sender == true);
        WALLET_CHECK(rtx->m_sender == false);

        // second transfer
        sw.start();
        txId = receiver.m_Wallet.transfer_money(receiver.m_WalletID, sender.m_WalletID, 6, 0, false, 200);
        mainReactor->run();
        sw.stop();
        cout << "Second transfer elapsed time: " << sw.milliseconds() << " ms\n";

        // check coins
        newSenderCoins = sender.GetCoins();
        newReceiverCoins = receiver.GetCoins();

        WALLET_CHECK(newSenderCoins.size() == 5);
        WALLET_CHECK(newReceiverCoins.size() == 2);

        WALLET_CHECK(newReceiverCoins[0].m_ID.m_Value == 4);
        WALLET_CHECK(newReceiverCoins[0].m_status == Coin::Available);
        WALLET_CHECK(newReceiverCoins[0].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newReceiverCoins[1].m_ID.m_Value == 6);
        WALLET_CHECK(newReceiverCoins[1].m_status == Coin::Available);
        WALLET_CHECK(newReceiverCoins[1].m_ID.m_Type == Key::Type::Regular);


        WALLET_CHECK(newSenderCoins[0].m_ID.m_Value == 3);
        WALLET_CHECK(newSenderCoins[0].m_status == Coin::Available);
        WALLET_CHECK(newSenderCoins[0].m_ID.m_Type == Key::Type::Change);

        WALLET_CHECK(newSenderCoins[1].m_ID.m_Value == 5);
        WALLET_CHECK(newSenderCoins[1].m_status == Coin::Spent);
        WALLET_CHECK(newSenderCoins[1].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[2].m_ID.m_Value == 2);
        WALLET_CHECK(newSenderCoins[2].m_status == Coin::Available);
        WALLET_CHECK(newSenderCoins[2].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[3].m_ID.m_Value == 1);
        WALLET_CHECK(newSenderCoins[3].m_status == Coin::Spent);
        WALLET_CHECK(newSenderCoins[3].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[4].m_ID.m_Value == 9);
        WALLET_CHECK(newSenderCoins[4].m_status == Coin::Spent);
        WALLET_CHECK(newSenderCoins[4].m_ID.m_Type == Key::Type::Regular);

        // Tx history check
        sh = sender.m_WalletDB->getTxHistory();
        WALLET_CHECK(sh.size() == 2);
        rh = receiver.m_WalletDB->getTxHistory();
        WALLET_CHECK(rh.size() == 2);
        stx = sender.m_WalletDB->getTx(txId);
        WALLET_CHECK(stx.is_initialized());
        rtx = receiver.m_WalletDB->getTx(txId);
        WALLET_CHECK(rtx.is_initialized());

        WALLET_CHECK(stx->m_txId == rtx->m_txId);
        WALLET_CHECK(stx->m_amount == rtx->m_amount);
        WALLET_CHECK(stx->m_status == TxStatus::Completed);
        WALLET_CHECK(stx->m_message == rtx->m_message);
        WALLET_CHECK(stx->m_createTime >= rtx->m_createTime);
        WALLET_CHECK(stx->m_status == rtx->m_status);
        WALLET_CHECK(stx->m_sender == true);
        WALLET_CHECK(rtx->m_sender == false);


        // third transfer. no enough money should appear
        sw.start();

        txId = receiver.m_Wallet.transfer_money(receiver.m_WalletID, sender.m_WalletID, 6, 0, false, 200);
        mainReactor->run();
        sw.stop();
        cout << "Third transfer elapsed time: " << sw.milliseconds() << " ms\n";
        // check coins
        newSenderCoins = sender.GetCoins();
        newReceiverCoins = receiver.GetCoins();

        // no coins 
        WALLET_CHECK(newSenderCoins.size() == 5);
        WALLET_CHECK(newReceiverCoins.size() == 2);

        // Tx history check. New failed tx should be added to sender and receiver
        sh = sender.m_WalletDB->getTxHistory();
        WALLET_CHECK(sh.size() == 3);
        rh = receiver.m_WalletDB->getTxHistory();
        WALLET_CHECK(rh.size() == 3);
        stx = sender.m_WalletDB->getTx(txId);
        WALLET_CHECK(stx.is_initialized());
        rtx = receiver.m_WalletDB->getTx(txId);
        WALLET_CHECK(rtx.is_initialized());

        WALLET_CHECK(rtx->m_amount == 6);
        WALLET_CHECK(rtx->m_status == TxStatus::Failed);
        WALLET_CHECK(rtx->m_sender == false);


        WALLET_CHECK(stx->m_amount == 6);
        WALLET_CHECK(stx->m_status == TxStatus::Failed);
        WALLET_CHECK(stx->m_sender == true);
    }

    void TestSwapTransaction()
    {
        cout << "\nTesting atomic swap transaction...\n";

        io::Reactor::Ptr mainReactor{ io::Reactor::create() };
        io::Reactor::Scope scope(*mainReactor);

        int completedCount = 2;
        auto f = [&completedCount, mainReactor](auto)
        {
            --completedCount;
            if (completedCount == 0)
            {
                mainReactor->stop();
                completedCount = 2;
            }
        };

        TestNode node;
        TestWalletRig sender("sender", createSenderWalletDB(), f);
        TestWalletRig receiver("receiver", createReceiverWalletDB(), f);

        /*TxID txID =*/ sender.m_Wallet.swap_coins(sender.m_WalletID, receiver.m_WalletID, 4, 1, wallet::AtomicSwapCoin::Bitcoin, 2);

        auto receiverCoins = receiver.GetCoins();
        WALLET_CHECK(receiverCoins.empty());

        mainReactor->run();

        receiverCoins = receiver.GetCoins();
        WALLET_CHECK(receiverCoins.size() == 1);
        WALLET_CHECK(receiverCoins[0].m_ID.m_Value == 4);
    }

    struct MyMmr : public Merkle::Mmr
    {
        typedef std::vector<Merkle::Hash> HashVector;
        typedef std::unique_ptr<HashVector> HashVectorPtr;

        std::vector<HashVectorPtr> m_vec;

        Merkle::Hash& get_At(const Merkle::Position& pos)
        {
            if (m_vec.size() <= pos.H)
                m_vec.resize(pos.H + 1);

            HashVectorPtr& ptr = m_vec[pos.H];
            if (!ptr)
                ptr.reset(new HashVector);


            HashVector& vec = *ptr;
            if (vec.size() <= pos.X)
                vec.resize(pos.X + 1);

            return vec[pos.X];
        }

        virtual void LoadElement(Merkle::Hash& hv, const Merkle::Position& pos) const override
        {
            hv = ((MyMmr*)this)->get_At(pos);
        }

        virtual void SaveElement(const Merkle::Hash& hv, const Merkle::Position& pos) override
        {
            get_At(pos) = hv;
        }
    };

    void TestSplitTransaction()
    {
        cout << "\nTesting split Tx...\n";

        io::Reactor::Ptr mainReactor{ io::Reactor::create() };
        io::Reactor::Scope scope(*mainReactor);

        auto senderWalletDB = createSqliteWalletDB("sender_wallet.db", false);

        // add coin with keyType - Coinbase
        beam::Amount coin_amount = 40;
        Coin coin = CreateAvailCoin(coin_amount, 0);
        coin.m_ID.m_Type = Key::Type::Coinbase;
        senderWalletDB->store(coin);

        auto coins = senderWalletDB->selectCoins(24);
        WALLET_CHECK(coins.size() == 1);
        WALLET_CHECK(coins[0].m_ID.m_Type == Key::Type::Coinbase);
        WALLET_CHECK(coins[0].m_status == Coin::Available);
        WALLET_CHECK(senderWalletDB->getTxHistory().empty());

        TestNode node;
        TestWalletRig sender("sender", senderWalletDB, [](auto) { io::Reactor::get_Current().stop(); });
        helpers::StopWatch sw;

        sw.start();
        auto txId = sender.m_Wallet.split_coins(sender.m_WalletID, { 11, 12, 13 }, 2, true, 200);
        mainReactor->run();
        sw.stop();

        cout << "Transfer elapsed time: " << sw.milliseconds() << " ms\n";

        // check Tx
        auto txHistory = senderWalletDB->getTxHistory();
        WALLET_CHECK(txHistory.size() == 1);
        WALLET_CHECK(txHistory[0].m_txId == txId);
        WALLET_CHECK(txHistory[0].m_amount == 36);
        WALLET_CHECK(txHistory[0].m_change == 2);
        WALLET_CHECK(txHistory[0].m_fee == 2);
        WALLET_CHECK(txHistory[0].m_status == TxStatus::Completed);

        // check coins
        vector<Coin> newSenderCoins;
        senderWalletDB->visit([&newSenderCoins](const Coin& c)->bool
        {
            newSenderCoins.push_back(c);
            return true;
        });

        WALLET_CHECK(newSenderCoins.size() == 5);
        WALLET_CHECK(newSenderCoins[0].m_ID.m_Type == Key::Type::Coinbase);
        WALLET_CHECK(newSenderCoins[0].m_status == Coin::Spent);
        WALLET_CHECK(newSenderCoins[0].m_ID.m_Value == 40);

        WALLET_CHECK(newSenderCoins[1].m_ID.m_Type == Key::Type::Change);
        WALLET_CHECK(newSenderCoins[1].m_status == Coin::Available);
        WALLET_CHECK(newSenderCoins[1].m_ID.m_Value == 2);

        WALLET_CHECK(newSenderCoins[2].m_ID.m_Type == Key::Type::Regular);
        WALLET_CHECK(newSenderCoins[2].m_status == Coin::Available);
        WALLET_CHECK(newSenderCoins[2].m_ID.m_Value == 11);

        WALLET_CHECK(newSenderCoins[3].m_ID.m_Type == Key::Type::Regular);
        WALLET_CHECK(newSenderCoins[3].m_status == Coin::Available);
        WALLET_CHECK(newSenderCoins[3].m_ID.m_Value == 12);

        WALLET_CHECK(newSenderCoins[4].m_ID.m_Type == Key::Type::Regular);
        WALLET_CHECK(newSenderCoins[4].m_status == Coin::Available);
        WALLET_CHECK(newSenderCoins[4].m_ID.m_Value == 13);

        cout << "\nFinish of testing split Tx...\n";
    }

    void TestExpiredTransaction()
    {
        cout << "\nTesting expired Tx...\n";

        io::Reactor::Ptr mainReactor{ io::Reactor::create() };
        io::Reactor::Scope scope(*mainReactor);

        int completedCount = 2;
        auto f = [&completedCount, mainReactor](auto)
        {
            --completedCount;
            if (completedCount == 0)
            {
                mainReactor->stop();
                completedCount = 2;
            }
        };

        TestNode node;
        TestWalletRig sender("sender", createSenderWalletDB(), f);
        TestWalletRig receiver("receiver", createReceiverWalletDB(), f);
        io::Timer::Ptr timer = io::Timer::create(*mainReactor);
        timer->start(1000, true, [&node]() {node.AddBlock(); });

        WALLET_CHECK(sender.m_WalletDB->selectCoins(6).size() == 2);
        WALLET_CHECK(sender.m_WalletDB->getTxHistory().empty());
        WALLET_CHECK(receiver.m_WalletDB->getTxHistory().empty());

        auto txId = sender.m_Wallet.transfer_money(sender.m_WalletID, receiver.m_WalletID, 4, 2, true, 1, 10);
        mainReactor->run();

        // first tx with height == 0
        {
            vector<Coin> newSenderCoins = sender.GetCoins();
            vector<Coin> newReceiverCoins = receiver.GetCoins();

            WALLET_CHECK(newSenderCoins.size() == 4);
            WALLET_CHECK(newReceiverCoins.size() == 0);

            auto sh = sender.m_WalletDB->getTxHistory();
            WALLET_CHECK(sh.size() == 1);
            WALLET_CHECK(sh[0].m_status == TxStatus::Failed);
            WALLET_CHECK(sh[0].m_failureReason == TxFailureReason::TransactionExpired);
            auto rh = receiver.m_WalletDB->getTxHistory();
            WALLET_CHECK(rh.size() == 1);
            WALLET_CHECK(rh[0].m_status == TxStatus::Failed);
            WALLET_CHECK(rh[0].m_failureReason == TxFailureReason::TransactionExpired);
        }

        //txId = sender.m_Wallet.transfer_money(sender.m_WalletID, receiver.m_WalletID, 4, 2, true, 0, 10);
        //mainReactor->run();

        //{
        //    vector<Coin> newSenderCoins = sender.GetCoins();
        //    vector<Coin> newReceiverCoins = receiver.GetCoins();

        //    WALLET_CHECK(newSenderCoins.size() == 4);
        //    WALLET_CHECK(newReceiverCoins.size() == 0);

        //    auto sh = sender.m_WalletDB->getTxHistory();
        //    WALLET_CHECK(sh.size() == 2);
        //    WALLET_CHECK(sh[0].m_status == TxStatus::Failed);
        //    WALLET_CHECK(sh[0].m_failureReason == TxFailureReason::TransactionExpired);
        //    auto rh = receiver.m_WalletDB->getTxHistory();
        //    WALLET_CHECK(rh.size() == 2);
        //    WALLET_CHECK(rh[0].m_status == TxStatus::Failed);
        //    WALLET_CHECK(rh[0].m_failureReason == TxFailureReason::TransactionExpired);
        //}

        txId = sender.m_Wallet.transfer_money(sender.m_WalletID, receiver.m_WalletID, 4, 2, true);

        mainReactor->run();

        {
            vector<Coin> newSenderCoins = sender.GetCoins();
            vector<Coin> newReceiverCoins = receiver.GetCoins();

            WALLET_CHECK(newSenderCoins.size() == 4);
            WALLET_CHECK(newReceiverCoins.size() == 1);

            auto sh = sender.m_WalletDB->getTxHistory();
            WALLET_CHECK(sh.size() == 2);
            auto sit = find_if(sh.begin(), sh.end(), [&txId](const auto& t) {return t.m_txId == txId; });
            WALLET_CHECK(sit->m_status == TxStatus::Completed);
            auto rh = receiver.m_WalletDB->getTxHistory();
            WALLET_CHECK(rh.size() == 2);
            auto rit = find_if(rh.begin(), rh.end(), [&txId](const auto& t) {return t.m_txId == txId; });
            WALLET_CHECK(rit->m_status == TxStatus::Completed);
        }
    }

    void TestTransactionUpdate()
    {
        cout << "\nTesting transaction update ...\n";

        io::Reactor::Ptr mainReactor{ io::Reactor::create() };
        io::Reactor::Scope scope(*mainReactor);
        struct TestGateway : wallet::INegotiatorGateway
        {
            void OnAsyncStarted() override {}
            void OnAsyncFinished() override {}
            void on_tx_completed(const TxID&) override {}
            void register_tx(const TxID&, Transaction::Ptr) override  {}
            void confirm_outputs(const std::vector<Coin>&) override  {}
            void confirm_kernel(const TxID&, const Merkle::Hash&) override  {}
            bool get_tip(Block::SystemState::Full& state) const override { return false; }
            void send_tx_params(const WalletID& peerID, wallet::SetTxParameter&&) override {}
            void UpdateOnNextTip(const TxID&) override {};
        } gateway;
        TestWalletRig sender("sender", createSenderWalletDB());
        TestWalletRig receiver("receiver", createReceiverWalletDB());

        TxID txID = wallet::GenerateTxID();
        auto tx = make_shared<wallet::SimpleTransaction>(gateway, sender.m_WalletDB, txID);
        Height currentHeight = sender.m_WalletDB->getCurrentHeight();

        tx->SetParameter(wallet::TxParameterID::TransactionType, wallet::TxType::Simple, false);
        tx->SetParameter(wallet::TxParameterID::MaxHeight, currentHeight + 2, false); // transaction is valid +lifetime blocks from currentHeight
        tx->SetParameter(wallet::TxParameterID::IsInitiator, true, false);
        //tx->SetParameter(wallet::TxParameterID::AmountList, {1U}, false);
      //  tx->SetParameter(wallet::TxParameterID::PreselectedCoins, {}, false);

        TxDescription txDescription;

        txDescription.m_txId = txID;
        txDescription.m_amount = 1;
        txDescription.m_fee = 2;
        txDescription.m_minHeight = currentHeight;
        txDescription.m_peerId = receiver.m_WalletID;
        txDescription.m_myId = sender.m_WalletID;
        txDescription.m_message = {};
        txDescription.m_createTime = getTimestamp();
        txDescription.m_sender = true;
        txDescription.m_status = TxStatus::Pending;
        txDescription.m_selfTx = false;
        sender.m_WalletDB->saveTx(txDescription);
        
        const int UpdateCount = 100000;
        helpers::StopWatch sw;
        sw.start();
        for (int i = 0; i < UpdateCount; ++i)
        {
            tx->Update();
        }
        sw.stop();

        cout << UpdateCount << " updates: " << sw.milliseconds() << " ms\n";

    }

    class PerformanceRig
    {
    public:
        PerformanceRig(int txCount, int txPerCall = 1)
            : m_TxCount(txCount)
            , m_TxPerCall(txPerCall)
        {

        }

        void Run()
        {
            io::Reactor::Ptr mainReactor{ io::Reactor::create() };
            io::Reactor::Scope scope(*mainReactor);

            int completedCount = 2 * m_TxCount;
            auto f = [&completedCount, mainReactor, count = 2 * m_TxCount](auto)
            {
                --completedCount;
                if (completedCount == 0)
                {
                    mainReactor->stop();
                    completedCount = count;
                }
            };

            TestNode node;
            TestWalletRig sender("sender", createSenderWalletDB(m_TxCount, 6), f);
            TestWalletRig receiver("receiver", createReceiverWalletDB(), f);

            io::Timer::Ptr timer = io::Timer::create(*mainReactor);
            auto timestamp = GetTime_ms();
            m_MaxLatency = 0;

            io::AsyncEvent::Ptr accessEvent;
            accessEvent = io::AsyncEvent::create(*mainReactor, [&timestamp, this, &accessEvent]()
            {
                auto newTimestamp = GetTime_ms();
                auto latency = newTimestamp - timestamp;
                timestamp = newTimestamp;
                if (latency > 100)
                {
                    cout << "Latency: " << float(latency) / 1000 << " s\n";
                }
                m_MaxLatency = max(latency, m_MaxLatency);
                accessEvent->post();
            });
            accessEvent->post();

            helpers::StopWatch sw;
            sw.start();

            io::Timer::Ptr sendTimer = io::Timer::create(*mainReactor);

            int sendCount = m_TxCount;
            io::AsyncEvent::Ptr sendEvent;
            sendEvent = io::AsyncEvent::create(*mainReactor, [&sender, &receiver, &sendCount, &sendEvent, this]()
            {
                for (int i = 0; i < m_TxPerCall && sendCount; ++i)
                {
                    if (sendCount--)
                    {
                        sender.m_Wallet.transfer_money(sender.m_WalletID, receiver.m_WalletID, 5, 1, true, 10000, 10000);
                    }
                }
                if (sendCount)
                {
                    sendEvent->post();
                }
            });
            sendEvent->post();

            mainReactor->run();
            sw.stop();
            m_TotalTime = sw.milliseconds();
        }

        uint64_t GetTotalTime() const
        {
            return m_TotalTime;
        }

        uint32_t GetMaxLatency() const
        {
            return m_MaxLatency;
        }

        int GetTxCount() const
        {
            return m_TxCount;
        }

        int GetTxPerCall() const
        {
            return m_TxPerCall;
        }


    private:
        int m_TxCount;
        int m_TxPerCall;
        uint32_t m_MaxLatency = 0;
        uint64_t m_TotalTime = 0;
    };

    void TestTxPerformance()
    {
        cout << "\nTesting tx performance...\n";

        const int MaxTxCount = 100;// 00;
        vector<PerformanceRig> tests;

        for (int i = 10; i <= MaxTxCount; i *= 10)
        {
            /*for (int j = 1; j <= 5; ++j)
            {
                tests.emplace_back(i, j);
            }*/
            tests.emplace_back(i, 1);
            tests.emplace_back(i, i);
        }

        for (auto& t : tests)
        {
            t.Run();
        }

        for (auto& t : tests)
        {
            cout << "Transferring of " << t.GetTxCount() << " by " << t.GetTxPerCall() << " transactions per call took: " << t.GetTotalTime() << " ms Max api latency: " << t.GetMaxLatency() << endl;
        }
    }

    void TestColdWalletSending()
    {
        cout << "\nTesting cold wallet sending...\n";

        io::Reactor::Ptr mainReactor{ io::Reactor::create() };
        io::Reactor::Scope scope(*mainReactor);

        int completedCount = 2;
        auto f = [&completedCount, mainReactor](auto)
        {
            --completedCount;
            if (completedCount == 0)
            {
                mainReactor->stop();
                completedCount = 2;
            }
        };

        TestNode node;
        TestWalletRig receiver("receiver", createReceiverWalletDB(), f);
        {
            TestWalletRig privateSender("sender", createSenderWalletDB(true), f, true);
            WALLET_CHECK(privateSender.m_WalletDB->selectCoins(6).size() == 2);
            WALLET_CHECK(privateSender.m_WalletDB->getTxHistory().empty());

            // send from cold wallet
            privateSender.m_Wallet.transfer_money(privateSender.m_WalletID, receiver.m_WalletID, 4, 2, true, 200);
            mainReactor->run();
        }

        string publicPath = "sender_public.db";
        {
            // cold -> hot
            boost::filesystem::remove(publicPath);
            boost::filesystem::copy_file(SenderWalletDB, publicPath);

            auto publicDB = WalletDB::open(publicPath, DBPassword, io::Reactor::get_Current().shared_from_this());
            TestWalletRig publicSender("public_sender", publicDB, f, false, true);

            WALLET_CHECK(publicSender.m_WalletDB->getTxHistory().size() == 1);
            WALLET_CHECK(receiver.m_WalletDB->getTxHistory().empty());

            mainReactor->run();
        }

        {
            // hot -> cold
            boost::filesystem::remove(SenderWalletDB);
            boost::filesystem::copy_file(publicPath, SenderWalletDB);
            auto privateDB = WalletDB::open(SenderWalletDB, DBPassword, io::Reactor::get_Current().shared_from_this());
            TestWalletRig privateSender("sender", privateDB, f, true);
            mainReactor->run();
        }

        // cold -> hot
        boost::filesystem::remove(publicPath);
        boost::filesystem::copy_file(SenderWalletDB, publicPath);

        auto publicDB = WalletDB::open(publicPath, DBPassword, io::Reactor::get_Current().shared_from_this());
        TestWalletRig publicSender("public_sender", publicDB, f);

        mainReactor->run();

        // check coins
        vector<Coin> newSenderCoins = publicSender.GetCoins();
        vector<Coin> newReceiverCoins = receiver.GetCoins();

        WALLET_CHECK(newSenderCoins.size() == 4);
        WALLET_CHECK(newReceiverCoins.size() == 1);
        WALLET_CHECK(newReceiverCoins[0].m_ID.m_Value == 4);
        WALLET_CHECK(newReceiverCoins[0].m_status == Coin::Available);
        WALLET_CHECK(newReceiverCoins[0].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[0].m_ID.m_Value == 5);
        WALLET_CHECK(newSenderCoins[0].m_status == Coin::Spent);
        WALLET_CHECK(newSenderCoins[0].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[1].m_ID.m_Value == 2);
        WALLET_CHECK(newSenderCoins[1].m_status == Coin::Available);
        WALLET_CHECK(newSenderCoins[1].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[2].m_ID.m_Value == 1);
        WALLET_CHECK(newSenderCoins[2].m_status == Coin::Spent);
        WALLET_CHECK(newSenderCoins[2].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[3].m_ID.m_Value == 9);
        WALLET_CHECK(newSenderCoins[3].m_status == Coin::Available);
        WALLET_CHECK(newSenderCoins[3].m_ID.m_Type == Key::Type::Regular);

    }


    void TestColdWalletReceiving()
    {
        cout << "\nTesting cold wallet receiving...\n";

        io::Reactor::Ptr mainReactor{ io::Reactor::create() };
        io::Reactor::Scope scope(*mainReactor);

        int completedCount = 2;
        auto f = [&completedCount, mainReactor](auto)
        {
            --completedCount;
            if (completedCount == 0)
            {
                mainReactor->stop();
                completedCount = 2;
            }
        };

        TestNode node;
        TestWalletRig sender("sender", createSenderWalletDB(), f);

        {
            // create cold wallet
            TestWalletRig privateReceiver("receiver", createReceiverWalletDB(true), f, true);
        }

        string publicPath = "receiver_public.db";
        {
            // cold -> hot
            boost::filesystem::remove(publicPath);
            boost::filesystem::copy_file(ReceiverWalletDB, publicPath);

            auto publicDB = WalletDB::open(publicPath, DBPassword, io::Reactor::get_Current().shared_from_this());
            TestWalletRig publicReceiver("public_receiver", publicDB, f, false, true);

            sender.m_Wallet.transfer_money(sender.m_WalletID, publicReceiver.m_WalletID, 4, 2, true, 200);

            mainReactor->run();
        }

        {
            // hot -> cold
            boost::filesystem::remove(ReceiverWalletDB);
            boost::filesystem::copy_file(publicPath, ReceiverWalletDB);
            auto privateDB = WalletDB::open(ReceiverWalletDB, DBPassword, io::Reactor::get_Current().shared_from_this());
            TestWalletRig privateReceiver("receiver", privateDB, f, true);
            mainReactor->run();
        }

        {
            // cold -> hot
            boost::filesystem::remove(publicPath);
            boost::filesystem::copy_file(ReceiverWalletDB, publicPath);

            auto publicDB = WalletDB::open(publicPath, DBPassword, io::Reactor::get_Current().shared_from_this());
            TestWalletRig publicReceiver("public_receiver", publicDB, f, false, true);

            mainReactor->run();
            mainReactor->run(); // to allow receiver complete this transaction
        }

        // hot -> cold
        boost::filesystem::remove(ReceiverWalletDB);
        boost::filesystem::copy_file(publicPath, ReceiverWalletDB);
        auto privateDB = WalletDB::open(ReceiverWalletDB, DBPassword, io::Reactor::get_Current().shared_from_this());
        TestWalletRig privateReceiver("receiver", privateDB, f, true);

        // check coins
        vector<Coin> newSenderCoins = sender.GetCoins();
        vector<Coin> newReceiverCoins = privateReceiver.GetCoins();

        WALLET_CHECK(newSenderCoins.size() == 4);
        WALLET_CHECK(newReceiverCoins.size() == 1);
        WALLET_CHECK(newReceiverCoins[0].m_ID.m_Value == 4);
        WALLET_CHECK(newReceiverCoins[0].m_status == Coin::Available);
        WALLET_CHECK(newReceiverCoins[0].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[0].m_ID.m_Value == 5);
        WALLET_CHECK(newSenderCoins[0].m_status == Coin::Spent);
        WALLET_CHECK(newSenderCoins[0].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[1].m_ID.m_Value == 2);
        WALLET_CHECK(newSenderCoins[1].m_status == Coin::Available);
        WALLET_CHECK(newSenderCoins[1].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[2].m_ID.m_Value == 1);
        WALLET_CHECK(newSenderCoins[2].m_status == Coin::Spent);
        WALLET_CHECK(newSenderCoins[2].m_ID.m_Type == Key::Type::Regular);

        WALLET_CHECK(newSenderCoins[3].m_ID.m_Value == 9);
        WALLET_CHECK(newSenderCoins[3].m_status == Coin::Available);
        WALLET_CHECK(newSenderCoins[3].m_ID.m_Type == Key::Type::Regular);

    }
}

int main()
{
    int logLevel = LOG_LEVEL_DEBUG;
#if LOG_VERBOSE_ENABLED
    logLevel = LOG_LEVEL_VERBOSE;
#endif
    auto logger = beam::Logger::create(logLevel, logLevel);
    Rules::get().FakePoW = true;
    Rules::get().UpdateChecksum();

    TestP2PWalletNegotiationST();
    //TestP2PWalletReverseNegotiationST();

    {
        io::Reactor::Ptr mainReactor{ io::Reactor::create() };
        io::Reactor::Scope scope(*mainReactor);
        //TestWalletNegotiation(CreateWalletDB<TestWalletDB>(), CreateWalletDB<TestWalletDB2>());
        TestWalletNegotiation(createSenderWalletDB(), createReceiverWalletDB());
    }

    TestSplitTransaction();

    //TestSwapTransaction();

    TestTxToHimself();

    //TestExpiredTransaction();

    TestTransactionUpdate();
    //TestTxPerformance();

    TestColdWalletSending();
    TestColdWalletReceiving();

    assert(g_failureCount == 0);
    return WALLET_CHECK_RESULT;
}
