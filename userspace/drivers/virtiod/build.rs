fn main() {
    println!("cargo:rustc-link-search=native=./../../../build/userspace/drivers/libdma/");
    println!("cargo:rustc-link-lib=dma");
    println!("cargo:rerun-if-changed=../../../proto/blockdev.proto");

    protobuf_codegen::Codegen::new()
        .include("../../../proto")
        .include("../../../build/nanopb_workdir/proto")
        .input("../../../proto/blockdev.proto")
        .input("../../../build/nanopb_workdir/proto/nanopb.proto")
        .input("../../../proto/mos_rpc.proto")
        .cargo_out_dir("protos")
        .run_from_script();
}
