"""Check the actual diagnostic bridge's native-reader ABI in a non-game host."""

import argparse
import ctypes
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("module", type=Path)
    args = parser.parse_args()
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
    }
    for name, (result, arguments) in signatures.items():
        function = getattr(bridge, name)
        function.restype = result
        function.argtypes = arguments
        check(bool(function), f"Missing ABI export {name}")

    check(bridge.DE_ApiVersion() == 2, "Additive diagnostic exports retain bridge ABI2")
    # A regular Python executable must never masquerade as the verified GTA host
    # or obtain engine access merely by loading this DLL and resolving its ABI.
    check(bridge.DE_BindRuntime() == 0, "Wrong executable must reject runtime binding")
    check(bridge.DE_NativePadBind() == 0, "Native binding requires the owning runtime")
    check(bridge.DE_NativePadCapture() == 0, "Unbound native capture must fail")
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
    check(bridge.DE_NativePadCapture() == 0, "Cleanup cannot create a native binding")
    for index in device_indices:
        check(bridge.DE_NativeDeviceRead(index) == 0, "Cleanup cannot retain or create raw device coverage")
    check(bridge.DE_ControllerCurrentMask() == 0, "Native diagnostics must not invent canonical controller coverage")
    print(f"Passed {checks} native pad bridge ABI checks")


if __name__ == "__main__":
    main()
