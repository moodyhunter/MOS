use std::env::args;

fn main() {
    println!("Hello, world from Rust on MOS.");

    for (i, arg) in args().enumerate() {
        println!("argv[{}]: {}", i, arg);
    }
}
