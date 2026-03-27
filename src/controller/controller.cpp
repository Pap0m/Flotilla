#include <filesystem>
#include <memory>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/ec.h>
#include <openssl/pem.h>
#include <stdexcept>

#define SERVICE_DIR "/etc/controller/"

#define ECC_TYPE "secp256k1"

void gen_key(std::filesystem::path& dir_path) {
  std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> ec_key(EVP_EC_gen(ECC_TYPE), EVP_PKEY_free);
  if (!ec_key) {
    throw std::runtime_error("Failed to gen key pair");
  }

  std::filesystem::path priv_path = dir_path / ECC_TYPE".key";
  std::filesystem::path pub_path = dir_path / ECC_TYPE".pem";
  std::unique_ptr<BIO, decltype(&BIO_free)> bio_priv(BIO_new_file(priv_path.c_str(), "w"), BIO_free); 
  if (!bio_priv || !PEM_write_bio_PrivateKey(bio_priv.get(), ec_key.get(), nullptr, nullptr, 0, nullptr, nullptr)) {
    throw std::runtime_error("Failed to write private key");
  }
  std::unique_ptr<BIO, decltype(&BIO_free)> bio_pub(BIO_new_file(pub_path.c_str(), "w"), BIO_free); 
  if (!bio_pub || !PEM_write_bio_PUBKEY(bio_pub.get(), ec_key.get())) {
    throw std::runtime_error("Failed to write public key");
  }

  std::filesystem::permissions(priv_path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace);
  std::filesystem::permissions(pub_path, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, std::filesystem::perm_options::replace);

  // gen_crt(ec_key.get(), SERVICE_DIR"tls/certificates");
}

int main() {
    
}
