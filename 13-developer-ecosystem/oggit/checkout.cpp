#include "checkout.hpp"

#include <filesystem>
#include <fstream>

namespace oggit {

namespace {

CheckoutError checkoutTreeInto(
    ObjectStore& store, const ObjectId& treeId, const std::filesystem::path& destination
) {
    ObjectType type;
    std::vector<uint8_t> content;
    StoreError readErr = store.readObject(treeId, type, content);
    if (readErr == StoreError::NotFound) {
        return CheckoutError::TreeNotFound;
    }
    if (readErr != StoreError::None) {
        return CheckoutError::ObjectReadError;
    }
    if (type != ObjectType::Tree) {
        return CheckoutError::NotATree;
    }

    std::vector<TreeEntry> entries;
    if (!parseTree(content, entries)) {
        return CheckoutError::ObjectReadError;
    }

    std::error_code ec;
    std::filesystem::create_directories(destination, ec);
    if (ec) {
        return CheckoutError::IoError;
    }

    for (const auto& entry : entries) {
        std::filesystem::path entryPath = destination / entry.name;

        if (entry.isDirectory) {
            CheckoutError childErr = checkoutTreeInto(store, entry.id, entryPath);
            if (childErr != CheckoutError::None) {
                return childErr;
            }
            continue;
        }

        ObjectType blobType;
        std::vector<uint8_t> blobContent;
        StoreError blobErr = store.readObject(entry.id, blobType, blobContent);
        if (blobErr != StoreError::None || blobType != ObjectType::Blob) {
            return CheckoutError::ObjectReadError;
        }

        std::ofstream out(entryPath, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return CheckoutError::IoError;
        }
        if (!blobContent.empty()) {
            out.write(reinterpret_cast<const char*>(blobContent.data()),
                       static_cast<std::streamsize>(blobContent.size()));
        }
        if (!out) {
            return CheckoutError::IoError;
        }
    }

    return CheckoutError::None;
}

}  // namespace

CheckoutError checkoutTree(
    ObjectStore& store, const ObjectId& treeId, const std::string& destinationDirectory
) {
    return checkoutTreeInto(store, treeId, std::filesystem::path(destinationDirectory));
}

}  // namespace oggit
