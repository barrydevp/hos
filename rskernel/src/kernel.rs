#![no_std] // don't link the Rust standard library
#![no_main] // disable all Rust-level entry points
#![feature(custom_test_frameworks)]
// #![test_runner(kernel_core::test_runner)]
#![reexport_test_harness_main = "test_main"]

use core::panic::PanicInfo;

static HELLO: &[u8] = b"Hello World!";

#[no_mangle] // don't mangle the name of this function
pub extern "C" fn kernel_main() -> ! {
    // this function is the entry point, since the linker looks for a function
    // named `_start` by default

    // ATTENTION: we have a very small stack and no guard page
    let video_memory = 0xb8000 as *mut u8;

    unsafe {
        *video_memory = 'A' as u8;
    }

    for (i, &byte) in HELLO.iter().enumerate() {
        unsafe {
            *video_memory.offset(i as isize * 2) = byte;
            *video_memory.offset(i as isize * 2 + 1) = 0xb;
        }
    }

    loop {}
}

/// This function for cpu halt
pub fn hlt_loop() -> ! {
    loop {
        // x86_64::instructions::hlt();
    }
}

/// This function is called on panic.
#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    hlt_loop()
}
