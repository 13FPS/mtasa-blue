/****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Reuse the existing MTA custom weapon-stat data and policy
 ****************************************************************************/
#pragma once

// Golden SDK values from the pre-extraction MTA table, SHA256
// 38c37533dbfb181f09137b8d73183122c06a71f35f255d6e190492bc564fd178.
// FNV-1a over explicitly serialized little-endian 32-bit SDK values; padding
// and compiler-specific record layout are deliberately excluded.
constexpr std::uint64_t goldenDefaults[3][21] = {
    {0xd178d697eca08f98ull, 0x671bb7571e5ed493ull, 0x1f95f7ef823595ebull, 0xbe0c4f886371b06eull, 0x72a75087c25bee08ull, 0x79cb7bb26fc36f8eull,
     0xab968a48dd7b0a41ull, 0xc746707d02b69debull, 0x12758f22c65059f4ull, 0x74335265c6d875d8ull, 0x98a29ad7fa0c4b29ull, 0x14d3936f5fbcc155ull,
     0x14d3936f5fbcc155ull, 0x14d3936f5fbcc155ull, 0x14d3936f5fbcc155ull, 0x14d3936f5fbcc155ull, 0x14d3936f5fbcc155ull, 0x14d3936f5fbcc155ull,
     0x14d3936f5fbcc155ull, 0x14d3936f5fbcc155ull, 0x14d3936f5fbcc155ull},
    {0xa2149c1a947a68beull, 0xe2f7a94d4a65de64ull, 0xfe7596c5b7c54a76ull, 0xc95a5a953ae24dffull, 0xa33cf3037e5b571cull, 0x830c9d05588ad849ull,
     0x83ea569ed171549dull, 0x32a254c0bb7a723cull, 0x1059ccfdedad15eeull, 0x72f7a16c24a9c273ull, 0x6f9f712a85d87b05ull, 0x512310ef5bf49231ull,
     0x5c73567eb87f5998ull, 0x9efe5976e909bb97ull, 0x5913b490fa410c98ull, 0xc193e15b81b49aeeull, 0xdab90d83f217d15aull, 0x4246bcdd49b253dbull,
     0xc7f27be74e0aa491ull, 0x07a1121b7f7c6147ull, 0x905add0d6087de6full},
    {0xb89bdc5e12dfa547ull, 0x000858e8a2194eacull, 0x5d50071b2c6333a0ull, 0xcf6bbcf8d8412245ull, 0xac0e02d0576d22f6ull, 0x178c32da37402991ull,
     0x19be898d7a6c6cd5ull, 0xefb4f61dcd172c4full, 0x10f781aea79120fcull, 0xdc4d21369007096aull, 0x731432a8de747601ull, 0x14d3936f5fbcc155ull,
     0x14d3936f5fbcc155ull, 0x14d3936f5fbcc155ull, 0x14d3936f5fbcc155ull, 0x14d3936f5fbcc155ull, 0x14d3936f5fbcc155ull, 0x14d3936f5fbcc155ull,
     0x14d3936f5fbcc155ull, 0x14d3936f5fbcc155ull, 0x14d3936f5fbcc155ull},
};
