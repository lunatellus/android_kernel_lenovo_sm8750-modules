Test:

si_core_tests include tests to check if si_core kernel functionality is working well or not.
We have five sets of tests included in the same:
1. Direct Path
	Kernel test to directly interact with si-core API for direct call.
2. Loading TA (from buffer)/Sending command to TA
	Kernel tests to interact with si-core API for loading TA (from buffer) and sending command.
3. Loading TA (from region)/Sending command to TA
	Kernel tests to interact with si-core API for loading TA (from region) and sending command.
4. Callback Object
	Kernel tests to interact with si-core API for creating callback objects from kernel.
5. Memory Object
	Kernel tests to interact with si-core API for creating memory objects from kernel.

Usage:

insmod /vendor_dlkm/lib/modules/si_core_test.ko
Push smcinvoke_example_ta64 from TZ APPS build to /vendor/firmware
Push tzecotestapp from TZ build to /vendor/firmware
echo > /dev/si_core_test_client
