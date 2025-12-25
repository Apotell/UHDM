#include "uhdm_db.hpp"
#include "utils/exceptions.hpp"

#include <uhdm/Serializer.h>


namespace uhdm_py {

UHDMDatabase::UHDMDatabase() 
    : serializer_(std::make_unique<uhdm::Serializer>()) {}

UHDMDatabase::~UHDMDatabase() = default;

UHDMDatabase::UHDMDatabase(UHDMDatabase&&) noexcept = default;
UHDMDatabase& UHDMDatabase::operator=(UHDMDatabase&&) noexcept = default;

void UHDMDatabase::load(const std::string& path) {
    try {
        serializer_->restore(path);
    } catch (const std::exception& e) {
        throw SerializationError(std::string("Failed to load UHDM file: ") + e.what());
    }
}

void UHDMDatabase::save(const std::string& path) const {
    try {
        // Note: save() is not const in UHDM Serializer, but logically
        // saving doesn't modify the design content. We use const_cast here.
        const_cast<uhdm::Serializer*>(serializer_.get())->save(path);
    } catch (const std::exception& e) {
        throw SerializationError(std::string("Failed to save UHDM file: ") + e.what());
    }
}


}  // namespace uhdm_py
