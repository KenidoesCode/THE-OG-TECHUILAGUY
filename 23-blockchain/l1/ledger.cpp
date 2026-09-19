#include "ledger.hpp"

#include "../../10-cryptography/hashing/sha256.hpp"

namespace l1 {

AccountState Ledger::get(const Address& addr) const {
    auto it = accounts.find(addr);
    if (it == accounts.end()) return AccountState{};
    return it->second;
}

TxResult Ledger::apply(const Transaction& tx) {
    AccountState sender = get(tx.from);

    if (tx.nonce != sender.nonce) {
        return TxResult::InvalidNonce;
    }
    if (sender.balance < tx.amount) {
        return TxResult::InsufficientBalance;
    }

    sender.balance -= tx.amount;
    sender.nonce += 1;
    accounts[tx.from] = sender;

    AccountState recipient = get(tx.to);
    recipient.balance += tx.amount;
    accounts[tx.to] = recipient;

    return TxResult::Ok;
}

Hash32 Ledger::computeStateRoot() const {
    crypto::Sha256 hasher;
    for (const auto& [addr, state] : accounts) {
        hasher.update(addr.data(), addr.size());
        uint8_t balanceBytes[8];
        uint8_t nonceBytes[8];
        for (int i = 0; i < 8; ++i) {
            balanceBytes[i] = static_cast<uint8_t>((state.balance >> (8 * (7 - i))) & 0xFF);
            nonceBytes[i] = static_cast<uint8_t>((state.nonce >> (8 * (7 - i))) & 0xFF);
        }
        hasher.update(balanceBytes, 8);
        hasher.update(nonceBytes, 8);
    }
    uint8_t digest[32];
    hasher.finish(digest);
    Hash32 hash;
    for (size_t i = 0; i < hash.size(); ++i) hash[i] = digest[i];
    return hash;
}

std::vector<std::pair<Address, AccountState>> Ledger::snapshot() const {
    std::vector<std::pair<Address, AccountState>> out;
    out.reserve(accounts.size());
    for (const auto& [addr, state] : accounts) {
        out.emplace_back(addr, state);
    }
    return out;
}

void Ledger::loadSnapshot(const std::vector<std::pair<Address, AccountState>>& entries) {
    accounts.clear();
    for (const auto& [addr, state] : entries) {
        accounts[addr] = state;
    }
}

}  // namespace l1
