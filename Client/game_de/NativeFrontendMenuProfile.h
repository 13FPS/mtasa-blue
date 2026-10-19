/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Genuine SA menu-owner and legacy frontend observation pins
 *****************************************************************************/
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace mta::game_de::NativeFrontendMenuProfile
{
    // A named native PauseMenu owns a UMG widget outside the legacy screen
    // stack. Both paths must be sampled; intro-movie storage is unrelated.
    // Source: mta-menu-presence-static-20261019-110130-4193a636;
    // profile SHA256 c5e979eb74f57f4973725afd9358cdcf9377e1e2ab2b8e54eaaef26559c9b39d.
    inline constexpr std::size_t    imageSize = 97141760;
    inline constexpr std::uintptr_t menuHeaderRva = 0x5069630;
    inline constexpr std::uintptr_t engineRootRva = 0x5725750;
    inline constexpr std::uintptr_t engineVtableRva = 0x431aea8;
    inline constexpr std::size_t    widgetOffset = 0x348;
    // The adjacent DWORD advances independently in live observations. Menu
    // presence uses only the proved widget pointer and never samples that scalar.
    inline constexpr std::size_t ownerSampleBytes = widgetOffset + sizeof(std::uint64_t);
    inline constexpr std::size_t maximumPinBytes = 94;

    inline constexpr std::uintptr_t                frontend_presence_predicateRva = 0x1152044;
    inline constexpr std::array<unsigned char, 26> frontend_presence_predicate = {0x44, 0x39, 0x25, 0xed, 0x75, 0xf1, 0x03, 0x4c, 0x89, 0xbc, 0x24, 0xb0, 0x00,
                                                                                  0x00, 0x00, 0x7f, 0x2a, 0x4c, 0x39, 0x25, 0xe4, 0x75, 0xf1, 0x03, 0x75, 0x21};

    inline constexpr std::uintptr_t                frontend_screen_pushRva = 0x11bf6d1;
    inline constexpr std::array<unsigned char, 58> frontend_screen_push = {
        0x8b, 0x3d, 0x61, 0x9f, 0xea, 0x03, 0x85, 0xff, 0x74, 0x21, 0x48, 0x8b, 0x15, 0x4e, 0x9f, 0xea, 0x03, 0x8d, 0x47, 0xff,
        0x4c, 0x8b, 0x03, 0x48, 0x63, 0xc8, 0x48, 0x8b, 0x14, 0xca, 0x48, 0x8b, 0xcb, 0x41, 0xff, 0x50, 0x18, 0x8b, 0x3d, 0x3c,
        0x9f, 0xea, 0x03, 0x8d, 0x47, 0x01, 0x3b, 0x05, 0x37, 0x9f, 0xea, 0x03, 0x89, 0x05, 0x2d, 0x9f, 0xea, 0x03};

    inline constexpr std::uintptr_t               frontend_pending_screen_storeRva = 0x11b96e5;
    inline constexpr std::array<unsigned char, 7> frontend_pending_screen_store = {0x48, 0x89, 0x0d, 0x54, 0xff, 0xea, 0x03};

    inline constexpr std::uintptr_t                frontend_screen_count_clearRva = 0x11b9707;
    inline constexpr std::array<unsigned char, 10> frontend_screen_count_clear = {0xc7, 0x05, 0x27, 0xff, 0xea, 0x03, 0x00, 0x00, 0x00, 0x00};

    inline constexpr std::uintptr_t                frontend_cleanupRva = 0x11c08e1;
    inline constexpr std::array<unsigned char, 16> frontend_cleanup = {0x40, 0x88, 0x3d, 0x0b, 0xb4, 0x06, 0x04, 0x48,
                                                                       0x89, 0x7b, 0x30, 0x39, 0x7b, 0x28, 0x74, 0x48};

    inline constexpr std::uintptr_t                frontend_texture_initializeRva = 0x11c0152;
    inline constexpr std::array<unsigned char, 94> frontend_texture_initialize = {
        0x48, 0x8b, 0x0d, 0xf7, 0x55, 0x56, 0x04, 0x48, 0x8d, 0x15, 0x78, 0xf3, 0x14, 0x03, 0x0f, 0x29, 0x7c, 0x24, 0x30, 0x44, 0x0f, 0x29, 0x44, 0x24,
        0x20, 0x48, 0x8b, 0x89, 0x68, 0x01, 0x00, 0x00, 0xe8, 0x39, 0x25, 0x9f, 0xff, 0x33, 0xdb, 0x48, 0x85, 0xc0, 0x74, 0x1d, 0x48, 0x8b, 0x0d, 0xcb,
        0x55, 0x56, 0x04, 0x4c, 0x8d, 0x05, 0x4c, 0xf3, 0x14, 0x03, 0x48, 0x8b, 0xd0, 0x4c, 0x8b, 0x09, 0x41, 0xff, 0x91, 0xe8, 0x02, 0x00, 0x00, 0xeb,
        0x03, 0x48, 0x8b, 0xc3, 0x48, 0x8b, 0x0d, 0xab, 0x55, 0x56, 0x04, 0x48, 0x8d, 0x15, 0x94, 0xf3, 0x14, 0x03, 0x48, 0x89, 0x47, 0x38};

    inline constexpr std::uintptr_t                frontend_texture_labelRva = 0x430f4d8;
    inline constexpr std::array<unsigned char, 11> frontend_texture_label = {0x6d, 0x65, 0x6e, 0x75, 0x5f, 0x62, 0x67, 0x6d, 0x61, 0x70, 0x00};

    inline constexpr std::uintptr_t               native_owner_root_referenceRva = 0x1209e1a;
    inline constexpr std::array<unsigned char, 7> native_owner_root_reference = {0x4c, 0x8b, 0x05, 0x2f, 0xb9, 0x51, 0x04};

    inline constexpr std::uintptr_t                concrete_sa_constructor_vtableRva = 0x13fad22;
    inline constexpr std::array<unsigned char, 22> concrete_sa_constructor_vtable = {0xe8, 0xf9, 0xa8, 0x6f, 0xff, 0x48, 0x8d, 0x05, 0x7a, 0x01, 0xf2,
                                                                                     0x02, 0xc7, 0x45, 0xe0, 0x00, 0x01, 0x02, 0x03, 0x48, 0x89, 0x03};

    inline constexpr std::uintptr_t                pause_menu_named_settings_openRva = 0x1297c89;
    inline constexpr std::array<unsigned char, 64> pause_menu_named_settings_open = {
        0x48, 0x8b, 0x9e, 0xb8, 0x04, 0x00, 0x00, 0x48, 0x8b, 0xcf, 0x48, 0x8b, 0xb6, 0xb0, 0x04, 0x00, 0x00, 0xe8, 0xb1, 0x7e, 0x87, 0xff,
        0x84, 0xc0, 0x74, 0x09, 0x48, 0x8b, 0x07, 0xff, 0x90, 0x20, 0x04, 0x00, 0x00, 0x48, 0x8b, 0x07, 0x41, 0xb9, 0x64, 0x00, 0x00, 0x00,
        0x4c, 0x8b, 0xc6, 0x40, 0x88, 0x6c, 0x24, 0x20, 0x48, 0x8b, 0xd3, 0x48, 0x8b, 0xcf, 0xff, 0x90, 0x00, 0x04, 0x00, 0x00};

    inline constexpr std::uintptr_t                menu_widget_publishRva = 0xafd054;
    inline constexpr std::array<unsigned char, 32> menu_widget_publish = {0x4c, 0x8b, 0x0e, 0x45, 0x33, 0xc0, 0x33, 0xd2, 0x44, 0x89, 0xa6,
                                                                          0x50, 0x03, 0x00, 0x00, 0x48, 0x8b, 0xce, 0x48, 0x89, 0xbe, 0x48,
                                                                          0x03, 0x00, 0x00, 0x41, 0xff, 0x91, 0xb0, 0x02, 0x00, 0x00};

    inline constexpr std::uintptr_t                menu_widget_presence_checkRva = 0xafdc5b;
    inline constexpr std::array<unsigned char, 20> menu_widget_presence_check = {0x48, 0x83, 0xb9, 0x48, 0x03, 0x00, 0x00, 0x00, 0x48, 0x8b,
                                                                                 0xf9, 0x48, 0x8b, 0x01, 0x0f, 0x84, 0xc9, 0x01, 0x00, 0x00};

    inline constexpr std::uintptr_t                menu_widget_close_and_clearRva = 0xafdc7f;
    inline constexpr std::array<unsigned char, 38> menu_widget_close_and_clear = {0x48, 0x8b, 0x8f, 0x48, 0x03, 0x00, 0x00, 0x48, 0x8b, 0x01, 0xff, 0x90, 0x88,
                                                                                  0x02, 0x00, 0x00, 0x33, 0xc0, 0x41, 0xb0, 0x01, 0x48, 0x89, 0x87, 0x48, 0x03,
                                                                                  0x00, 0x00, 0x41, 0x0f, 0xb6, 0xd0, 0x89, 0x87, 0x50, 0x03, 0x00, 0x00};

    inline constexpr std::uintptr_t               resume_named_virtual_dispatchRva = 0xc447ae;
    inline constexpr std::array<unsigned char, 9> resume_named_virtual_dispatch = {0x48, 0x8b, 0x01, 0xff, 0x90, 0xf8, 0x04, 0x00, 0x00};

    inline constexpr std::uintptr_t                resume_sa_menu_close_dispatchRva = 0x12980de;
    inline constexpr std::array<unsigned char, 21> resume_sa_menu_close_dispatch = {0x83, 0x3d, 0x13, 0x8f, 0x06, 0x04, 0x09, 0x48, 0x8b, 0xcf, 0x48,
                                                                                    0x8b, 0x07, 0x75, 0x4e, 0xff, 0x90, 0x48, 0x04, 0x00, 0x00};

    inline constexpr std::uintptr_t                resume_sa_base_menu_close_callRva = 0x1297efd;
    inline constexpr std::array<unsigned char, 24> resume_sa_base_menu_close_call = {0xc6, 0x05, 0xef, 0x3d, 0xf9, 0x03, 0x00, 0xe8, 0x37, 0x5d, 0x86, 0xff,
                                                                                     0x0f, 0xb6, 0xd8, 0x84, 0xc0, 0x74, 0x48, 0xe8, 0xcb, 0x8e, 0xf2, 0xff};

    inline constexpr std::uintptr_t                resume_game_from_menu_nameRva = 0x4233af0;
    inline constexpr std::array<unsigned char, 19> resume_game_from_menu_name = {0x52, 0x65, 0x73, 0x75, 0x6d, 0x65, 0x47, 0x61, 0x6d, 0x65,
                                                                                 0x46, 0x72, 0x6f, 0x6d, 0x4d, 0x65, 0x6e, 0x75, 0x00};

    inline constexpr std::uintptr_t                pause_menu_class_nameRva = 0x428ede8;
    inline constexpr std::array<unsigned char, 15> pause_menu_class_name = {0x50, 0x61, 0x75, 0x73, 0x65, 0x4d, 0x65, 0x6e,
                                                                            0x75, 0x43, 0x6c, 0x61, 0x73, 0x73, 0x00};

    inline constexpr std::uintptr_t                pause_menu_template_nameRva = 0x428edd0;
    inline constexpr std::array<unsigned char, 20> pause_menu_template_name = {0x50, 0x61, 0x75, 0x73, 0x65, 0x4d, 0x65, 0x6e, 0x75, 0x55,
                                                                               0x49, 0x54, 0x65, 0x6d, 0x70, 0x6c, 0x61, 0x74, 0x65, 0x00};

    inline constexpr std::uintptr_t               pause_menu_class_offsetRva = 0x4290c24;
    inline constexpr std::array<unsigned char, 4> pause_menu_class_offset = {0xb8, 0x04, 0x00, 0x00};

    inline constexpr std::uintptr_t               pause_menu_template_offsetRva = 0x4290ac4;
    inline constexpr std::array<unsigned char, 4> pause_menu_template_offset = {0xb0, 0x04, 0x00, 0x00};

    struct Pin
    {
        std::uintptr_t       rva;
        const unsigned char* bytes;
        std::size_t          size;
        bool                 code;
    };
    inline constexpr std::array<Pin, 21> pins = {{
        {frontend_presence_predicateRva, frontend_presence_predicate.data(), frontend_presence_predicate.size(), true},
        {frontend_screen_pushRva, frontend_screen_push.data(), frontend_screen_push.size(), true},
        {frontend_pending_screen_storeRva, frontend_pending_screen_store.data(), frontend_pending_screen_store.size(), true},
        {frontend_screen_count_clearRva, frontend_screen_count_clear.data(), frontend_screen_count_clear.size(), true},
        {frontend_cleanupRva, frontend_cleanup.data(), frontend_cleanup.size(), true},
        {frontend_texture_initializeRva, frontend_texture_initialize.data(), frontend_texture_initialize.size(), true},
        {frontend_texture_labelRva, frontend_texture_label.data(), frontend_texture_label.size(), false},
        {native_owner_root_referenceRva, native_owner_root_reference.data(), native_owner_root_reference.size(), true},
        {concrete_sa_constructor_vtableRva, concrete_sa_constructor_vtable.data(), concrete_sa_constructor_vtable.size(), true},
        {pause_menu_named_settings_openRva, pause_menu_named_settings_open.data(), pause_menu_named_settings_open.size(), true},
        {menu_widget_publishRva, menu_widget_publish.data(), menu_widget_publish.size(), true},
        {menu_widget_presence_checkRva, menu_widget_presence_check.data(), menu_widget_presence_check.size(), true},
        {menu_widget_close_and_clearRva, menu_widget_close_and_clear.data(), menu_widget_close_and_clear.size(), true},
        {resume_named_virtual_dispatchRva, resume_named_virtual_dispatch.data(), resume_named_virtual_dispatch.size(), true},
        {resume_sa_menu_close_dispatchRva, resume_sa_menu_close_dispatch.data(), resume_sa_menu_close_dispatch.size(), true},
        {resume_sa_base_menu_close_callRva, resume_sa_base_menu_close_call.data(), resume_sa_base_menu_close_call.size(), true},
        {resume_game_from_menu_nameRva, resume_game_from_menu_name.data(), resume_game_from_menu_name.size(), false},
        {pause_menu_class_nameRva, pause_menu_class_name.data(), pause_menu_class_name.size(), false},
        {pause_menu_template_nameRva, pause_menu_template_name.data(), pause_menu_template_name.size(), false},
        {pause_menu_class_offsetRva, pause_menu_class_offset.data(), pause_menu_class_offset.size(), false},
        {pause_menu_template_offsetRva, pause_menu_template_offset.data(), pause_menu_template_offset.size(), false},
    }};

    struct VtableTarget
    {
        std::uintptr_t slotRva, targetRva;
    };
    // Loaded pointers are checked as image-relative targets after relocation.
    // These five entries qualify the concrete native menu open/close dispatch.
    inline constexpr std::array<VtableTarget, 5> vtableTargets = {{
        {0x431b2a8, 0xafcd80},
        {0x431b2b8, 0x1297bc0},
        {0x431b2f0, 0x1297ef0},
        {0x431b3a0, 0x12980d0},
        {0x431b3a8, 0x1298180},
    }};

    struct ImagePointer
    {
        std::uintptr_t slotRva, targetRva;
        bool           code;
    };
    inline constexpr std::array<ImagePointer, 4> imagePointers = {{
        {0x423b020, 0x4233af0, false},
        {0x423b028, 0xc44790, true},
        {0x4290c00, 0x428ede8, false},
        {0x4290aa0, 0x428edd0, false},
    }};
}
