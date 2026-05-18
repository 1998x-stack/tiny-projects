use thiserror::Error;

pub type PageId = u32;

#[derive(Error, Debug, PartialEq, Eq)]
pub enum DbError {
    #[error("page not found: {0}")]
    PageNotFound(PageId),

    #[error("key not found")]
    KeyNotFound,

    #[error("page is full")]
    PageFull,

    #[error("checksum mismatch on page {0}")]
    ChecksumMismatch(PageId),

    #[error("I/O error: {0}")]
    IoError(String),
}

pub type Result<T> = std::result::Result<T, DbError>;
