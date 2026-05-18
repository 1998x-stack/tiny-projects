use std::collections::HashMap;
use std::sync::{Mutex, atomic::{AtomicU32, Ordering}};
use crate::{PageId, Result, DbError};
use crate::storage::page::Page;

pub struct MemoryPageStore {
    pages: Mutex<HashMap<PageId, Page>>,
    next_id: AtomicU32,
    freelist: Mutex<Vec<PageId>>,
}

impl MemoryPageStore {
    pub fn new() -> Self {
        Self {
            pages: Mutex::new(HashMap::new()),
            next_id: AtomicU32::new(1),
            freelist: Mutex::new(Vec::new()),
        }
    }

    pub fn read_page(&self, id: PageId) -> Result<Page> {
        let pages = self.pages.lock().unwrap();
        pages.get(&id).cloned().ok_or(DbError::PageNotFound(id))
    }

    pub fn write_page(&self, page: &Page) -> Result<()> {
        let mut pages = self.pages.lock().unwrap();
        pages.insert(page.id, page.clone());
        Ok(())
    }

    pub fn allocate_page(&self) -> Result<PageId> {
        if let Some(id) = self.freelist.lock().unwrap().pop() {
            return Ok(id);
        }
        let id = self.next_id.fetch_add(1, Ordering::SeqCst);
        Ok(id)
    }

    pub fn free_page(&self, id: PageId) -> Result<()> {
        let mut pages = self.pages.lock().unwrap();
        pages.remove(&id);
        self.freelist.lock().unwrap().push(id);
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::storage::page::{PageType, Page};

    #[test]
    fn test_allocate_and_read_back() {
        let store = MemoryPageStore::new();
        let id = store.allocate_page().unwrap();
        assert_eq!(id, 1);

        let mut page = Page::new(id, PageType::Leaf);
        page.insert_leaf_cell(0, b"hello", b"world");
        store.write_page(&page).unwrap();

        let read = store.read_page(id).unwrap();
        assert_eq!(read.id, id);
        assert_eq!(read.num_cells(), 1);
        assert_eq!(read.leaf_cell_at(0).unwrap().0, b"hello");
    }

    #[test]
    fn test_free_and_reuse() {
        let store = MemoryPageStore::new();
        let id1 = store.allocate_page().unwrap();
        let _id2 = store.allocate_page().unwrap();
        store.free_page(id1).unwrap();
        let id3 = store.allocate_page().unwrap();
        assert_eq!(id3, id1);
    }

    #[test]
    fn test_read_nonexistent_page() {
        let store = MemoryPageStore::new();
        let result = store.read_page(999);
        assert!(matches!(result.unwrap_err(), DbError::PageNotFound(999)));
    }
}
