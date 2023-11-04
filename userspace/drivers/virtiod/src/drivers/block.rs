use virtio_drivers::{device::blk::VirtIOBlk, transport::Transport};

use crate::hal::MOSHal;

pub fn start_block_driver<T: Transport>(transport: T) {
    let mut blk = VirtIOBlk::<MOSHal, T>::new(transport).expect("failed to create blk driver");

    assert!(!blk.readonly());
    let mut input = [0xffu8; 512];
    let mut output = [0; 512];
    for i in 0..32 {
        let mut j = 0;
        for x in input.iter_mut() {
            *x = j as u8;
            j += 1;
        }

        blk.write_blocks(i, &input).expect("failed to write");
        blk.read_blocks(i, &mut output).expect("failed to read");
        assert_eq!(input, output);
        println!("read/write block {} passed", i);
    }
}
