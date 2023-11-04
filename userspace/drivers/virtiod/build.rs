fn main() {
    println!("cargo:rustc-link-search=native=./../../../build/userspace/drivers/libdma/");
    println!("cargo:rustc-link-lib=dma_hosted");
}
