#pragma once
// Shared content-check digest for the desktop clients and dedicated servers.
#include <cstddef>
#if defined(__APPLE__) && !defined(KFX_SHA256_USE_OPENSSL)
#include <CommonCrypto/CommonDigest.h>
using KfxSha256Context = CC_SHA256_CTX;
inline void KfxSha256Init(KfxSha256Context* c){CC_SHA256_Init(c);}
inline void KfxSha256Update(KfxSha256Context* c,const void* p,size_t n){CC_SHA256_Update(c,p,(CC_LONG)n);}
inline void KfxSha256Final(unsigned char* p,KfxSha256Context* c){CC_SHA256_Final(p,c);}
#elif defined(_WIN32) && !defined(KFX_SHA256_USE_OPENSSL)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <bcrypt.h>
#include <stdexcept>
struct KfxSha256Context {
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE hash=nullptr;
    ~KfxSha256Context(){
        if(hash)BCryptDestroyHash(hash);
        if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
    }
    KfxSha256Context()=default;
    KfxSha256Context(const KfxSha256Context&)=delete;
    KfxSha256Context& operator=(const KfxSha256Context&)=delete;
};
inline void KfxSha256Init(KfxSha256Context* c){
    if(BCryptOpenAlgorithmProvider(&c->algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0||
       BCryptCreateHash(c->algorithm,&c->hash,nullptr,0,nullptr,0,0)<0)
        throw std::runtime_error("SHA-256 initialization failed");
}
inline void KfxSha256Update(KfxSha256Context* c,const void* p,size_t n){
    if(BCryptHashData(c->hash,const_cast<unsigned char*>(static_cast<const unsigned char*>(p)),
        static_cast<ULONG>(n),0)<0)
        throw std::runtime_error("SHA-256 update failed");
}
inline void KfxSha256Final(unsigned char* p,KfxSha256Context* c){
    if(BCryptFinishHash(c->hash,p,32,0)<0)
        throw std::runtime_error("SHA-256 finalization failed");
}
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
