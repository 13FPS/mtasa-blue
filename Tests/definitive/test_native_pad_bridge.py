"""Check the actual diagnostic bridge's native-reader ABI in a non-game host."""

import argparse
import ctypes
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("module", type=Path)
    args = parser.parse_args()
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.GetModuleHandleW.restype = ctypes.c_void_p
    kernel32.GetModuleHandleW.argtypes = [ctypes.c_wchar_p]
    renderer_before = {
        name: kernel32.GetModuleHandleW(name)
        for name in ("d3dx9_43.dll", "d3d9.dll")
    }
    bridge = ctypes.CDLL(str(args.module.resolve(strict=True)))
    checks = 0

    def check(condition, why):
        nonlocal checks
        checks += 1
        if not condition:
            raise RuntimeError(why)

    signatures = {
        "DE_ApiVersion": (ctypes.c_uint32, []),
        "DE_BindRuntime": (ctypes.c_int, []),
        "DE_NativePadBind": (ctypes.c_int, []),
        "DE_NativePadCapture": (ctypes.c_int, []),
        "DE_NativePadRead": (ctypes.c_uint32, [ctypes.c_uint32] * 3),
        "DE_NativeDeviceRead": (ctypes.c_uint32, [ctypes.c_uint32]),
        "DE_NativePadReset": (None, []),
        "DE_Disconnect": (None, []),
        "DE_ControllerCurrentMask": (ctypes.c_uint32, []),
        "DE_MtaMovementBegin": (ctypes.c_int, []),
        "DE_MtaMovementSubmit": (ctypes.c_int, [ctypes.c_uint32]),
        "DE_MtaMovementCapture": (ctypes.c_int, []),
        "DE_MtaMovementRead": (ctypes.c_uint32, [ctypes.c_uint32]),
        "DE_MtaMovementEnd": (ctypes.c_int, []),
        "DE_CoreStartupBegin": (ctypes.c_int, []),
        "DE_CoreStartupPoll": (ctypes.c_int, []),
        "DE_CoreStartupCancel": (ctypes.c_int, []),
        "DE_CoreStartupRead": (ctypes.c_uint32, [ctypes.c_uint32]),
        "DE_CoreInputBegin": (ctypes.c_int, []),
        "DE_CoreInputCapture": (ctypes.c_int, []),
        "DE_CoreInputRead": (ctypes.c_uint32, [ctypes.c_uint32]),
    }
    for name, (result, arguments) in signatures.items():
        function = getattr(bridge, name)
        function.restype = result
        function.argtypes = arguments
        check(bool(function), f"Missing ABI export {name}")

    check(bridge.DE_ApiVersion() == 2, "Additive diagnostic exports retain bridge ABI2")
    check(getattr(bridge, "DE_CoreStartupRun", None) is None, "Unsafe synchronous startup export must be absent")
    # A regular Python executable must never masquerade as the verified GTA host
    # or obtain engine access merely by loading this DLL and resolving its ABI.
    check(bridge.DE_BindRuntime() == 0, "Wrong executable must reject runtime binding")
    # The startup diagnostic loads the real adjacent Core only in its verified
    # owning game runtime. A rejected host must retain an empty cache and must
    # not load Core merely because all diagnostic exports can be resolved.
    for name in ("core.dll", "game_de.dll"):
        check(not kernel32.GetModuleHandleW(name), f"Rejected host starts without {name}")
    check(bridge.DE_CoreStartupBegin() == 0, "Wrong executable cannot begin Core startup")
    check(bridge.DE_CoreInputBegin() == 0, "Wrong executable cannot begin configured Core input")
    check(bridge.DE_CoreInputCapture() == 0, "Rejected host cannot freeze an input sample")
    for component in (*range(64), 64, 0xFFFFFFFF):
        check(bridge.DE_CoreInputRead(component) == 0, "Rejected host cannot manufacture configured input")
    check(bridge.DE_CoreStartupPoll() == -1, "Rejected startup cannot publish a worker status")
    check(bridge.DE_CoreStartupCancel() == 0, "Rejected startup cannot acquire worker cancellation ownership")
    for component in (*range(19), 19, 0xFFFFFFFF):
        check(bridge.DE_CoreStartupRead(component) == 0, "Rejected startup cannot manufacture cached diagnostics")
    check(bridge.DE_CoreStartupBegin() == 0, "Repeated rejected startup remains unavailable")
    check(bridge.DE_CoreStartupPoll() == -1, "Repeated rejection never manufactures terminal completion")
    for name in ("core.dll", "game_de.dll"):
        check(not kernel32.GetModuleHandleW(name), f"Rejected startup cannot load {name}")
    for name, handle in renderer_before.items():
        check(kernel32.GetModuleHandleW(name) == handle, f"Rejected startup cannot load legacy renderer {name}")
    check(bridge.DE_NativePadBind() == 0, "Native binding requires the owning runtime")
    check(bridge.DE_NativePadCapture() == 0, "Unbound native capture must fail")
    check(bridge.DE_MtaMovementBegin() == 0, "Wrong executable cannot activate movement writes")
    check(bridge.DE_MtaMovementCapture() == 0, "Rejected trial cannot manufacture native metrics")
    for phase in (0, 1, 2, 3, 4, 0xFFFFFFFF):
        check(bridge.DE_MtaMovementSubmit(phase) == 0, "Unbound action submission must fail")
    for component in (*range(12), 12, 0xFFFFFFFF):
        check(bridge.DE_MtaMovementRead(component) == 0, "Unbound movement metrics remain unavailable")
    check(bridge.DE_MtaMovementEnd() == 1, "Unstarted trial cleanup is idempotent")
    check(bridge.DE_MtaMovementEnd() == 1, "Repeated unstarted cleanup is idempotent")
    for pad in (0, 1, 2, 0xFFFFFFFF):
        for which in (0, 1, 2, 0xFFFFFFFF):
            for index in (0, 16, 23, 24, 0xFFFFFFFF):
                check(bridge.DE_NativePadRead(pad, which, index) == 0, "Unbound component read must remain unavailable")
    # Every raw source and the range boundary remain unavailable after rejected
    # binding/capture; loading the bridge cannot manufacture an input snapshot.
    device_indices = (*range(127), 127, 128, 0xFFFFFFFF)
    for index in device_indices:
        check(bridge.DE_NativeDeviceRead(index) == 0, "Unbound or invalid device source must remain unavailable")
    bridge.DE_NativePadReset()
    bridge.DE_Disconnect()
    check(bridge.DE_CoreInputBegin() == 0, "Cleanup cannot authorize configured Core input")
    check(bridge.DE_CoreInputCapture() == 0, "Cleanup cannot manufacture configured input coverage")
    for component in (*range(64), 64, 0xFFFFFFFF):
        check(bridge.DE_CoreInputRead(component) == 0, "Cleanup cannot retain configured input coverage")
    check(bridge.DE_CoreStartupBegin() == 0, "Cleanup cannot authorize Core startup")
    check(bridge.DE_CoreStartupCancel() == 0, "Cleanup cannot create cancellation ownership")
    check(bridge.DE_CoreStartupPoll() == -1, "Cleanup cannot create worker completion")
    for component in (*range(19), 19, 0xFFFFFFFF):
        check(bridge.DE_CoreStartupRead(component) == 0, "Cleanup cannot retain or create startup diagnostics")
    for name in ("core.dll", "game_de.dll"):
        check(not kernel32.GetModuleHandleW(name), f"Cleanup cannot load {name}")
    check(bridge.DE_NativePadCapture() == 0, "Cleanup cannot create a native binding")
    for index in device_indices:
        check(bridge.DE_NativeDeviceRead(index) == 0, "Cleanup cannot retain or create raw device coverage")
    check(bridge.DE_ControllerCurrentMask() == 0, "Native diagnostics must not invent canonical controller coverage")
    print(f"Passed {checks} native pad bridge ABI checks")


if __name__ == "__main__":
    main()
