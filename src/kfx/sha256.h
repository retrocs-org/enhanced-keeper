#pragma once
// Shared content-check digest. CommonCrypto on macOS, OpenSSL EVP on servers.
#include <cstddef>
#if defined(__APPLE__) && !defined(KFX_SHA256_USE_OPENSSL)
#include <CommonCrypto/CommonDigest.h>
using KfxSha256Context = CC_SHA256_CTX;
inline void KfxSha256Init(KfxSha256Context* c){CC_SHA256_Init(c);}
inline void KfxSha256Update(KfxSha256Context* c,const void* p,size_t n){CC_SHA256_Update(c,p,(CC_LONG)n);}
inline void KfxSha256Final(unsigned char* p,KfxSha256Context* c){CC_SHA256_Final(p,c);}
#else
#include <openssl/evp.h>
#include <stdexcept>
struct KfxSha256Context {
    EVP_MD_CTX* value=EVP_MD_CTX_new();
    ~KfxSha256Context(){EVP_MD_CTX_free(value);}
    KfxSha256Context()=default;
    KfxSha256Context(const KfxSha256Context&)=delete;
    KfxSha256Context& operator=(const KfxSha256Context&)=delete;
};
inline void KfxSha256Init(KfxSha256Context* c){if(!c->value||EVP_DigestInit_ex(c->value,EVP_sha256(),nullptr)!=1)throw std::runtime_error("SHA-256 initialization failed");}
inline void KfxSha256Update(KfxSha256Context* c,const void* p,size_t n){if(EVP_DigestUpdate(c->value,p,n)!=1)throw std::runtime_error("SHA-256 update failed");}
inline void KfxSha256Final(unsigned char* p,KfxSha256Context* c){unsigned n=0;if(EVP_DigestFinal_ex(c->value,p,&n)!=1||n!=32)throw std::runtime_error("SHA-256 finalization failed");}
#endif
