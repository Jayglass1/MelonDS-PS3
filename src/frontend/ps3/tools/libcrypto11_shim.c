// Compatibility shim: provides the OpenSSL 1.1.0 symbols the ps3dev signing
// tools import (AES_*, SHA1*, CRYPTO_ctr128_encrypt) by forwarding to the
// system libcrypto.so.3, which still exports them. Loaded via LD_LIBRARY_PATH
// under the name libcrypto.so.1.1.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stddef.h>
#include <stdlib.h>

static void* Real;
static void* Sym(const char* name)
{
    if (!Real) {
        Real = dlopen("libcrypto.so.3", RTLD_NOW | RTLD_LOCAL);
        if (!Real) abort();
    }
    void* p = dlsym(Real, name);
    if (!p) abort();
    return p;
}

int AES_set_encrypt_key(const unsigned char* k, const int bits, void* key)
{ static int (*f)(const unsigned char*, const int, void*); if (!f) f = Sym("AES_set_encrypt_key"); return f(k, bits, key); }

void AES_encrypt(const unsigned char* in, unsigned char* out, const void* key)
{ static void (*f)(const unsigned char*, unsigned char*, const void*); if (!f) f = Sym("AES_encrypt"); f(in, out, key); }

void CRYPTO_ctr128_encrypt(const unsigned char* in, unsigned char* out, size_t len, const void* key,
                           unsigned char ivec[16], unsigned char ecount_buf[16], unsigned int* num, void* block)
{ static void (*f)(const unsigned char*, unsigned char*, size_t, const void*, unsigned char*, unsigned char*, unsigned int*, void*);
  if (!f) f = Sym("CRYPTO_ctr128_encrypt");
  f(in, out, len, key, ivec, ecount_buf, num, block); }

unsigned char* SHA1(const unsigned char* d, size_t n, unsigned char* md)
{ static unsigned char* (*f)(const unsigned char*, size_t, unsigned char*); if (!f) f = Sym("SHA1"); return f(d, n, md); }

int SHA1_Init(void* c)
{ static int (*f)(void*); if (!f) f = Sym("SHA1_Init"); return f(c); }

int SHA1_Update(void* c, const void* d, size_t n)
{ static int (*f)(void*, const void*, size_t); if (!f) f = Sym("SHA1_Update"); return f(c, d, n); }

int SHA1_Final(unsigned char* md, void* c)
{ static int (*f)(unsigned char*, void*); if (!f) f = Sym("SHA1_Final"); return f(md, c); }
