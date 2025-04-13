#[macro_export]
macro_rules! data_slice_and_shift {
    ($data:ident, $newvar:ident, $size:expr) => {
        if $data.len() < $size {
            return Err(Box::new(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "invalid data in rpc arg",
            )));
        }
        let $newvar = $data[..$size].to_vec();
        $data = $data[$size..].to_vec();
    };
}

#[macro_export]
macro_rules! do_try_into {
    ($data:ident, $from:expr, $to:expr) => {
        $data[$from..$to].try_into().or_else(|_| {
            Err(std::io::Error::new(
                std::io::ErrorKind::InvalidData,
                "invalid data in rpc arg",
            ))
        })?
    };
}
