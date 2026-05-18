use std::sync::Arc;
use crate::{Result};
use crate::storage::page::{Page, PageType};
use crate::storage::mem_store::MemoryPageStore;

pub struct BPlusTree {
    pub root_page_id: u32,
    pub store: Arc<MemoryPageStore>,
    pub leaf_max_cells: usize,
    pub inner_max_cells: usize,
}

impl BPlusTree {
    pub fn new(store: Arc<MemoryPageStore>) -> Self {
        Self {
            root_page_id: 0,
            store,
            leaf_max_cells: super::page::LEAF_MAX_CELLS,
            inner_max_cells: super::page::INNER_MAX_CELLS,
        }
    }

    pub fn lookup(&self, key: &[u8]) -> Result<Option<Vec<u8>>> {
        if self.root_page_id == 0 {
            return Ok(None);
        }
        let mut page = self.store.read_page(self.root_page_id)?;
        while page.page_type() == PageType::Inner {
            let child_id = page.child_page_id_for_key(key);
            page = self.store.read_page(child_id)?;
        }
        let idx = page.lower_bound(key);
        if let Some((k, v)) = page.leaf_cell_at(idx) {
            if &k[..] == key {
                return Ok(Some(v));
            }
        }
        Ok(None)
    }

    pub fn insert(&mut self, key: &[u8], value: &[u8]) -> Result<()> {
        if self.root_page_id == 0 {
            let page_id = self.store.allocate_page()?;
            let mut leaf = Page::new(page_id, PageType::Leaf);
            leaf.insert_leaf_cell(0, key, value);
            self.store.write_page(&leaf)?;
            self.root_page_id = page_id;
            return Ok(());
        }

        let mut path: Vec<(u32, Page)> = Vec::new();
        let mut page = self.store.read_page(self.root_page_id)?;
        while page.page_type() == PageType::Inner {
            let child_id = page.child_page_id_for_key(key);
            path.push((page.id, page));
            page = self.store.read_page(child_id)?;
        }

        let idx = page.lower_bound(key) as u16;
        if (page.num_cells() as usize) < self.leaf_max_cells {
            let mut leaf = self.store.read_page(page.id)?;
            leaf.insert_leaf_cell(idx, key, value);
            self.store.write_page(&leaf)?;
            return Ok(());
        }

        let mut leaf = self.store.read_page(page.id)?;
        let (mut left, mut right, separator_key) = self.split_leaf(&mut leaf, idx, key, value);
        let mut right_page_id = right.id;
        let mut sep = separator_key;

        loop {
            match path.pop() {
                None => {
                    let new_root_id = self.store.allocate_page()?;
                    let mut new_root = Page::new(new_root_id, PageType::Inner);
                    new_root.set_first_child_page_id(left.id);
                    new_root.insert_inner_cell(0, &sep, right_page_id);
                    self.store.write_page(&new_root)?;
                    self.root_page_id = new_root_id;
                    self.store.write_page(&left)?;
                    self.store.write_page(&right)?;
                    return Ok(());
                }
                Some((_parent_id, mut parent)) => {
                    if (parent.num_cells() as usize) < self.inner_max_cells {
                        parent.insert_inner_cell(parent.num_cells(), &sep, right_page_id);
                        self.store.write_page(&parent)?;
                        self.store.write_page(&left)?;
                        self.store.write_page(&right)?;
                        return Ok(());
                    }
                    let result = self.split_inner(&mut parent, &sep, right_page_id);
                    left = result.0;
                    right = result.1;
                    sep = result.2;
                    right_page_id = right.id;
                }
            }
        }
    }

    fn split_leaf(&self, leaf: &mut Page, insert_idx: u16, key: &[u8], value: &[u8]) -> (Page, Page, Vec<u8>) {
        let new_page_id = self.store.allocate_page().unwrap();
        let mut new_leaf = Page::new(new_page_id, PageType::Leaf);

        let mid = ((self.leaf_max_cells + 1) / 2) as u16;

        leaf.insert_leaf_cell(insert_idx, key, value);

        for i in (mid..leaf.num_cells()).rev() {
            let (k, v) = leaf.leaf_cell_at(i).unwrap();
            new_leaf.insert_leaf_cell(0, &k, &v);
            leaf.remove_leaf_cell(i);
        }

        let sep = new_leaf.leaf_cell_at(0).unwrap().0;
        new_leaf.header_mut().next_leaf = leaf.header().next_leaf;
        leaf.header_mut().next_leaf = new_page_id;

        (leaf.clone(), new_leaf, sep)
    }

    fn split_inner(&self, inner: &mut Page, insert_key: &[u8], child_page_id: u32) -> (Page, Page, Vec<u8>) {
        let new_page_id = self.store.allocate_page().unwrap();
        let mut new_inner = Page::new(new_page_id, PageType::Inner);

        let mid = ((self.inner_max_cells + 1) / 2) as u16;

        let ins_idx = inner.lower_bound(insert_key);
        inner.insert_inner_cell(ins_idx, insert_key, child_page_id);

        let sep = inner.inner_cell_at(mid as u16).unwrap().0;

        new_inner.set_first_child_page_id(
            inner.inner_cell_at(mid as u16).map(|(_, id)| id).unwrap_or(0)
        );
        inner.remove_inner_cell(mid as u16);

        for i in ((mid as u16)..inner.num_cells()).rev() {
            let (k, c) = inner.inner_cell_at(i).unwrap();
            new_inner.insert_inner_cell(0, &k, c);
            inner.remove_inner_cell(i);
        }

        (inner.clone(), new_inner, sep)
    }

    pub fn delete(&mut self, key: &[u8]) -> Result<()> {
        if self.root_page_id == 0 { return Ok(()); }

        let mut page = self.store.read_page(self.root_page_id)?;
        while page.page_type() == PageType::Inner {
            let child_id = page.child_page_id_for_key(key);
            page = self.store.read_page(child_id)?;
        }

        let idx = page.lower_bound(key);
        if let Some((k, _)) = page.leaf_cell_at(idx) {
            if &k[..] == key {
                let mut leaf = self.store.read_page(page.id)?;
                leaf.remove_leaf_cell(idx);
                self.store.write_page(&leaf)?;
            }
        }
        Ok(())
    }

    pub fn range_scan(&self, start: &[u8], end: &[u8]) -> Result<Vec<(Vec<u8>, Vec<u8>)>> {
        let mut results = Vec::new();
        if self.root_page_id == 0 { return Ok(results); }

        let mut page = self.store.read_page(self.root_page_id)?;
        while page.page_type() == PageType::Inner {
            let child_id = page.child_page_id_for_key(start);
            page = self.store.read_page(child_id)?;
        }

        loop {
            for i in 0..page.num_cells() {
                let (key, val) = page.leaf_cell_at(i).unwrap();
                if &key[..] > end { return Ok(results); }
                if &key[..] >= start { results.push((key, val)); }
            }
            let next_id = page.header().next_leaf;
            if next_id == 0 { break; }
            page = self.store.read_page(next_id)?;
        }
        Ok(results)
    }

    pub fn verify_leaf_depth(&self) -> Result<usize> {
        if self.root_page_id == 0 { return Ok(0); }
        let root = self.store.read_page(self.root_page_id)?;
        let mut depths = Vec::new();
        self.collect_leaf_depths(&root, 0, &mut depths)?;
        let first = depths.first().copied().unwrap_or(0);
        for d in &depths {
            if *d != first {
                return Err(crate::DbError::IoError(
                    format!("leaf depth mismatch: expected {}, found {}", first, d)));
            }
        }
        Ok(first)
    }

    fn collect_leaf_depths(&self, page: &Page, depth: usize, depths: &mut Vec<usize>) -> Result<()> {
        if page.page_type() == PageType::Leaf {
            depths.push(depth);
            return Ok(());
        }
        if page.first_child_page_id() != 0 {
            let child = self.store.read_page(page.first_child_page_id())?;
            self.collect_leaf_depths(&child, depth + 1, depths)?;
        }
        for i in 0..page.num_cells() {
            if let Some((_, child_id)) = page.inner_cell_at(i) {
                if child_id != 0 {
                    let child = self.store.read_page(child_id)?;
                    self.collect_leaf_depths(&child, depth + 1, depths)?;
                }
            }
        }
        Ok(())
    }

    pub fn verify_node_capacity(&self) -> Result<()> {
        if self.root_page_id == 0 { return Ok(()); }
        let root = self.store.read_page(self.root_page_id)?;
        self.verify_node_capacity_recursive(&root)
    }

    fn verify_node_capacity_recursive(&self, page: &Page) -> Result<()> {
        let max = if page.page_type() == PageType::Leaf { self.leaf_max_cells } else { self.inner_max_cells };
        if page.num_cells() as usize > max {
            return Err(crate::DbError::IoError(
                format!("page {} has {} cells, max is {}", page.id, page.num_cells(), max)));
        }
        if page.page_type() == PageType::Inner {
            if page.first_child_page_id() != 0 {
                let child = self.store.read_page(page.first_child_page_id())?;
                self.verify_node_capacity_recursive(&child)?;
            }
            for i in 0..page.num_cells() {
                if let Some((_, child_id)) = page.inner_cell_at(i) {
                    if child_id != 0 {
                        let child = self.store.read_page(child_id)?;
                        self.verify_node_capacity_recursive(&child)?;
                    }
                }
            }
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::sync::Arc;

    fn setup() -> BPlusTree {
        BPlusTree::new(Arc::new(MemoryPageStore::new()))
    }

    #[test]
    fn test_empty_tree_lookup() {
        let tree = setup();
        assert!(tree.lookup(b"anything").unwrap().is_none());
    }

    #[test]
    fn test_insert_single_and_lookup() {
        let mut tree = setup();
        tree.insert(b"key1", b"val1").unwrap();
        assert_eq!(tree.lookup(b"key1").unwrap(), Some(b"val1".to_vec()));
    }

    #[test]
    fn test_lookup_missing_key() {
        let mut tree = setup();
        tree.insert(b"key1", b"val1").unwrap();
        assert!(tree.lookup(b"key2").unwrap().is_none());
    }

    #[test]
    fn test_leaf_split() {
        let mut tree = setup();
        for i in 0..(tree.leaf_max_cells + 5) as u32 {
            let key = format!("key{:03}", i);
            tree.insert(key.as_bytes(), &i.to_be_bytes()).unwrap();
        }
        for i in 0..(tree.leaf_max_cells + 5) as u32 {
            let key = format!("key{:03}", i);
            let val = tree.lookup(key.as_bytes()).unwrap().unwrap();
            assert_eq!(val, i.to_be_bytes());
        }
    }

    #[test]
    fn test_root_split() {
        let mut tree = setup();
        let n = tree.leaf_max_cells + 1;
        for i in 0..n as u32 {
            tree.insert(&i.to_be_bytes(), &i.to_be_bytes()).unwrap();
        }
        let root = tree.store.read_page(tree.root_page_id).unwrap();
        assert_eq!(root.page_type(), PageType::Inner);
        assert!(root.num_cells() >= 1);
        for i in 0..n as u32 {
            assert_eq!(tree.lookup(&i.to_be_bytes()).unwrap().unwrap(), i.to_be_bytes());
        }
    }

    #[test]
    fn test_delete() {
        let mut tree = setup();
        tree.insert(b"key1", b"val1").unwrap();
        tree.insert(b"key2", b"val2").unwrap();
        tree.delete(b"key1").unwrap();
        assert!(tree.lookup(b"key1").unwrap().is_none());
        assert_eq!(tree.lookup(b"key2").unwrap().unwrap(), b"val2");
    }

    #[test]
    fn test_delete_nonexistent() {
        let mut tree = setup();
        tree.insert(b"key1", b"val1").unwrap();
        tree.delete(b"key2").unwrap();
        assert_eq!(tree.lookup(b"key1").unwrap().unwrap(), b"val1");
    }

    #[test]
    fn test_range_scan() {
        let mut tree = setup();
        for i in 0..5u32 {
            tree.insert(&i.to_be_bytes(), &i.to_be_bytes()).unwrap();
        }
        let results = tree.range_scan(&2u32.to_be_bytes(), &4u32.to_be_bytes()).unwrap();
        assert_eq!(results.len(), 3);
    }

    #[test]
    fn test_range_scan_across_leaves() {
        let mut tree = setup();
        let n = tree.leaf_max_cells * 2;
        for i in 0..n as u32 {
            tree.insert(&i.to_be_bytes(), &i.to_be_bytes()).unwrap();
        }
        let start = (tree.leaf_max_cells - 2) as u32;
        let end = (tree.leaf_max_cells + 2) as u32;
        let results = tree.range_scan(&start.to_be_bytes(), &end.to_be_bytes()).unwrap();
        assert_eq!(results.len(), 5);
    }

    #[test]
    fn test_invariants_after_random_ops() {
        let mut tree = setup();
        for i in 0..500u32 {
            tree.insert(&i.to_be_bytes(), &i.to_be_bytes()).unwrap();
        }
        tree.verify_leaf_depth().unwrap();
        tree.verify_node_capacity().unwrap();
    }
}
