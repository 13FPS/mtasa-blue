/*****************************************************************************
 * PROJECT: Multi Theft Auto
 * LICENSE: See LICENSE in the repository root
 * PURPOSE: Exercise optional movement persistence through real owned Core widgets
 *****************************************************************************/
#pragma once

// Included inside the owned host's anonymous namespace after its GUI helpers.
// These paths belong to one disposable fixture; the default user profile is
// deliberately never opted in while testing file transactions.
std::filesystem::path BindingFixturePath(const std::filesystem::path& root)
{
    return root / L"binding-fixture/definitive/keybinds.xml";
}

std::string ReadBindingFixture(const std::filesystem::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    Check(stream.good(), "read owned binding fixture bytes");
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

void WriteBindingFixture(const std::filesystem::path& path, const std::string& bytes)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    Check(stream.good(), "write only the explicitly owned invalid-load fixture");
}

void CheckMovementFile(const std::string& bytes)
{
    constexpr std::array<const char*, 6> controls{"forwards", "backwards", "left", "right", "sprint", "walk"};
    constexpr std::array<const char*, 6> keys{"i", "k", "j", "l", "u", "o"};
    Check(bytes.find("<definitive_keybinds version=\"1\"") != std::string::npos && bytes.find("<binds>") != std::string::npos &&
              bytes.find("</definitive_keybinds>") != std::string::npos,
          "atomic Apply writes the generated versioned movement document");
    std::array<unsigned, 6> found{};
    unsigned                records = 0;
    for (std::size_t cursor = 0; (cursor = bytes.find("<bind ", cursor)) != std::string::npos;)
    {
        const auto end = bytes.find('>', cursor);
        Check(end != std::string::npos, "every generated movement record has a complete element");
        const auto record = bytes.substr(cursor, end - cursor + 1);
        bool       matched = false;
        for (std::size_t index = 0; index < controls.size(); ++index)
        {
            if (record.find(std::string("control=\"") + controls[index] + '"') == std::string::npos)
                continue;
            Check(record.find(std::string("key=\"") + keys[index] + '"') != std::string::npos && !found[index]++,
                  "generated movement record retains its applied key exactly once");
            matched = true;
        }
        Check(matched && record.find("command=") == std::string::npos && record.find("arguments=") == std::string::npos,
              "durable XML contains only the admitted movement controls");
        ++records;
        cursor = end + 1;
    }
    Check(records == controls.size() && std::all_of(found.begin(), found.end(), [](auto count) { return count == 1; }) &&
              bytes.find("host_preserve") == std::string::npos && bytes.find("control=\"fire\"") == std::string::npos,
          "all six movements persist while unrelated command and control binds stay outside the durable file");
}

void BindingPersistenceNonRuntime(Owned& owned, const std::filesystem::path& root)
{
    const auto enable = Export<BindingPersistence::Enable>(owned.core, "MTA_EnableDEFrontendBindingPersistenceV1");
    const auto atPath = Export<BindingPersistence::EnableAtPath>(owned.core, "MTA_EnableDEFrontendBindingPersistenceAtPathV1");
    const auto path = BindingFixturePath(root).wstring();
    Check(enable(owned.frontend, BindingPersistence::ApiVersion) == Frontend::Status::Busy &&
              atPath(owned.frontend, BindingPersistence::ApiVersion, path.data(), static_cast<std::uint32_t>(path.size())) == Frontend::Status::Busy &&
              !std::filesystem::exists(path),
          "ordinary frontend refuses persistence before selecting or writing any user or fixture file");
}

void BindingPersistenceSettings(Owned& owned, const std::filesystem::path& root, unsigned view, bool restarted)
{
    const auto enable = Export<BindingPersistence::Enable>(owned.core, "MTA_EnableDEFrontendBindingPersistenceV1");
    const auto atPath = Export<BindingPersistence::EnableAtPath>(owned.core, "MTA_EnableDEFrontendBindingPersistenceAtPathV1");
    const auto readBindings = Export<Bindings::Read>(owned.core, "MTA_ReadDEFrontendBindings");
    const auto file = BindingFixturePath(root);
    const auto path = file.wstring();
    const auto call = [&](void* handle, const std::filesystem::path& selected, std::uint32_t version = BindingPersistence::ApiVersion)
    {
        const auto wide = selected.wstring();
        return atPath(handle, version, wide.data(), static_cast<std::uint32_t>(wide.size()));
    };
    const auto state = [&]
    {
        Bindings::Snapshot values{};
        Check(readBindings(owned.frontend, values.data(), Bindings::FieldCount) == Frontend::Status::Ok,
              "read unchanged Bindings32 around optional durable admission");
        return values;
    };
    const auto before = state();
    if (view == 1 && !restarted)
    {
        Check(before[Bindings::ConfigurationReadOnly] && RuntimeState(owned)[Runtime::ConfigurationReadOnly] && !std::filesystem::exists(file),
              "runtime remains read-only by default and has not created a binding file");
        const auto unknown = reinterpret_cast<void*>(std::uintptr_t{1});
        Check(enable(nullptr, 1) == Frontend::Status::InvalidArgument && enable(unknown, 1) == Frontend::Status::InvalidArgument &&
                  enable(owned.frontend, 0) == Frontend::Status::InvalidArgument && enable(owned.frontend, 2) == Frontend::Status::InvalidArgument &&
                  call(nullptr, file) == Frontend::Status::InvalidArgument && call(unknown, file) == Frontend::Status::InvalidArgument &&
                  call(owned.startup, file) == Frontend::Status::InvalidArgument && call(owned.frontend, file, 0) == Frontend::Status::InvalidArgument &&
                  call(owned.frontend, file, 2) == Frontend::Status::InvalidArgument,
              "optional persistence rejects null unknown runtime-owner and foreign-version identities");
        Check(atPath(owned.frontend, 1, nullptr, 0) == Frontend::Status::InvalidArgument &&
                  atPath(owned.frontend, 1, path.data(), 0) == Frontend::Status::InvalidArgument &&
                  call(owned.frontend, L"definitive/keybinds.xml") == Frontend::Status::InvalidArgument &&
                  call(owned.frontend, root / L"binding-fixture/definitive/other.xml") == Frontend::Status::InvalidArgument &&
                  call(owned.frontend, root / L"binding-fixture/other/keybinds.xml") == Frontend::Status::InvalidArgument &&
                  call(owned.frontend, root / L"mta/config/coreconfig.xml") == Frontend::Status::InvalidArgument,
              "explicit persistence refuses malformed nonabsolute and non-dedicated configuration paths");
        bool        foreign = false;
        std::thread other(
            [&] { foreign = call(owned.frontend, file) == Frontend::Status::WrongThread && enable(owned.frontend, 1) == Frontend::Status::WrongThread; });
        other.join();
        Check(foreign && state() == before && !std::filesystem::exists(file),
              "foreign persistence calls preserve the real manager diagnostics and create no file");
        const auto invalid = root / L"invalid-binding-fixture/definitive/keybinds.xml";
        for (const auto& bytes : {std::string("<definitive_keybinds"),
                                  std::string("<definitive_keybinds version=\"1\"><binds><bind key=\"x\" control=\"fire\"/></binds></definitive_keybinds>"),
                                  std::string("<definitive_keybinds version=\"2\"><binds/></definitive_keybinds>")})
        {
            WriteBindingFixture(invalid, bytes);
            Check(call(owned.frontend, invalid) == Frontend::Status::Failed && ReadBindingFixture(invalid) == bytes && state() == before,
                  "malformed unknown-control and unknown-schema XML fail without changing disk or the configured manager");
        }
        bindingPersistenceAdmissionVerified = true;
    }
    std::filesystem::create_directories(file.parent_path());
    Check(call(owned.frontend, file) == Frontend::Status::Ok && call(owned.frontend, file) == Frontend::Status::Ok,
          "explicit opt-in selects the owned movement path idempotently before Settings opens");
    Check(call(owned.frontend, root / L"other-binding-fixture/definitive/keybinds.xml") == Frontend::Status::Busy &&
              !state()[Bindings::ConfigurationReadOnly] && !RuntimeState(owned)[Runtime::ConfigurationReadOnly] &&
              !owned.State()[Frontend::ConfigurationReadOnly],
          "explicit opt-in alone changes frontend bindings and runtime read-only diagnostics while refusing a second destination");
    if (view == 1 && !restarted)
        Check(!std::filesystem::exists(file), "opting in alone does not create the durable file before Apply");

    auto& observed = *owned.observed;
    auto* window = observed.settingsWindow;
    auto* grid = observed.settingsGrid;
    Check(window && grid && observed.settingsApply && observed.settingsCancel, "durable view reuses the actual MTA Settings window grid and callbacks");
    const auto process = Export<Console::ProcessMessage>(owned.core, "MTA_ProcessDEFrontendConsoleMessage");
    const auto message = [&](UINT type, WPARAM key)
    {
        std::uint32_t handled = UINT32_MAX;
        Check(process(owned.frontend, type, key, 0, &handled) == Frontend::Status::Ok && handled <= 1,
              "durable Settings capture uses the existing frontend message route");
        return handled != 0;
    };
    const auto button = [&](CGUIButton* value)
    {
        CVector2D point = value->GetSize() / 2;
        for (auto* element = static_cast<CGUIElement*>(value); element; element = element->GetParent())
            point += element->GetPosition();
        Click(owned, static_cast<int>(point.fX), static_cast<int>(point.fY));
    };
    const auto open = [&]
    {
        Click(owned, 230, 716);
        Check(window->IsVisible(), "actual menu opens the durable movement Settings window");
    };
    constexpr std::array<const char*, 6> controls{"forwards", "backwards", "left", "right", "sprint", "walk"};
    constexpr std::array<const char*, 6> defaults{"w", "s", "a", "d", "lshift", "rshift"};
    constexpr std::array<const char*, 6> applied{"i", "k", "j", "l", "u", "o"};
    constexpr std::array<WPARAM, 6>      keys{'I', 'K', 'J', 'L', 'U', 'O'};
    std::array<int, 6>                   rows{};
    const auto                           inspect = [&](const auto& expected)
    {
        rows.fill(-1);
        unsigned count = 0;
        for (int row = 0; row < grid->GetRowCount(); ++row)
        {
            const auto type = static_cast<KeyBindType>(reinterpret_cast<std::intptr_t>(grid->GetItemData(row, 1)));
            if (type != KeyBindType::GTA_CONTROL)
                continue;
            auto* bind = static_cast<CGTAControlBind*>(grid->GetItemData(row, 2));
            Check(bind && bind->control && bind->boundKey, "durable grid borrows a currently live real MTA bind record");
            const auto action =
                std::find_if(controls.begin(), controls.end(), [&](const char* name) { return std::strcmp(name, bind->control->szControl) == 0; });
            Check(action != controls.end(), "durable Settings exposes only the six admitted movement controls");
            const auto index = static_cast<std::size_t>(action - controls.begin());
            Check(rows[index] == -1 && grid->GetItemText(row, 2) == std::string(expected[index]) && std::strcmp(bind->boundKey->szKey, expected[index]) == 0,
                  "fresh grid and real manager agree on every configured movement key");
            rows[index] = row;
            ++count;
        }
        Check(count == controls.size() && grid->GetRowCount() == 7, "nonmovement and command fixture bindings never appear in the constrained Settings grid");
    };
    const auto capture = [&](std::size_t index, WPARAM key)
    {
        grid->SetSelectedItem(rows[index], 2, true);
        Check(grid->Event_OnDoubleClick() && message(WM_KEYDOWN, key), "real durable Settings capture stages the requested key");
        message(WM_KEYUP, key);
    };
    open();
    Check(call(owned.frontend, file) == Frontend::Status::Busy, "open Settings excludes persistence reload behind its borrowed edit records");
    if (view == 1 && !restarted)
    {
        inspect(defaults);
        for (std::size_t index = 0; index < controls.size(); ++index)
            capture(index, keys[index]);
        Check(!std::filesystem::exists(file), "staged key edits also leave the absent durable file untouched");
        button(observed.settingsApply);
        Check(!window->IsVisible(), "successful atomic movement Apply closes the real Settings dialog");
        open();
        // Apply can replace the real CKeyBinds clone. Rebuild and borrow its new
        // rows before observing keys; never dereference pre-commit grid data.
        inspect(applied);
        button(observed.settingsCancel);
    }
    else
    {
        inspect(applied);
        button(observed.settingsCancel);
    }
    const auto saved = ReadBindingFixture(file);
    CheckMovementFile(saved);
    Check(state()[Bindings::BindCount] == before[Bindings::BindCount] && state()[Bindings::BindCount] == 8 &&
              state()[Bindings::IgnoredBindings] == before[Bindings::IgnoredBindings] && state()[Bindings::IgnoredBindings] == 1,
          "Apply and reload preserve the base manager's unrelated control and command binding counts");
    open();
    inspect(applied);
    capture(0, 'P');
    if (view == 1 && !restarted)
    {
        auto*      unchangedBind = static_cast<CGTAControlBind*>(grid->GetItemData(rows[0], 2));
        const auto lock = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Check(lock != INVALID_HANDLE_VALUE, "hold only the owned destination without delete sharing to force an atomic replacement failure");
        button(observed.settingsApply);
        const bool retained = window->IsVisible() && grid->GetItemData(rows[0], 2) == unchangedBind && std::strcmp(unchangedBind->boundKey->szKey, "i") == 0 &&
                              ReadBindingFixture(file) == saved;
        Check(CloseHandle(lock) != FALSE, "release the fixture-only replacement failure handle");
        Check(retained, "failed atomic save keeps Settings open and preserves both prior file bytes and the live manager record");
        // Dismiss the ordinary Core error box through genuine GUI keyboard
        // handling, then Cancel the still-staged edit rather than retry Apply.
        owned.gui->ProcessKeyboardInput(CGUIKeys::Return, true);
        owned.gui->ProcessKeyboardInput(CGUIKeys::Return, false);
        bindingPersistenceSaveFailureVerified = true;
    }
    button(observed.settingsCancel);
    Check(!window->IsVisible() && ReadBindingFixture(file) == saved, "Cancel leaves the last successful durable document byte-for-byte unchanged");
    open();
    inspect(applied);
    button(observed.settingsCancel);
    if (view == 2)
    {
        open();
        inspect(applied);
        grid->SetSelectedItem(rows[0], 2, true);
        Check(grid->Event_OnDoubleClick() && message(WM_CHAR, 'P'), "durable view retains a genuine modal capture for normal hide cleanup");
        settingsCaptureRetainedForCleanup = true;
        bindingPersistenceVerified = true;
        if (restarted)
            bindingPersistenceRestartVerified = true;
    }
}
