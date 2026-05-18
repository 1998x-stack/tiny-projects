use crate::PageId;

pub const PAGE_SIZE: usize = 4096;
pub const HEADER_SIZE: usize = 16;

pub const INNER_CELL_SIZE: usize = 2 + 8 + 4;
pub const LEAF_CELL_SIZE: usize = 2 + 8 + 2 + 256;
pub const INNER_MAX_CELLS: usize = (PAGE_SIZE - HEADER_SIZE) / INNER_CELL_SIZE;
pub const LEAF_MAX_CELLS: usize = (PAGE_SIZE - HEADER_SIZE) / LEAF_CELL_SIZE;

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PageType {
    Inner = 0,
    Leaf = 1,
}

impl PageType {
    pub fn from_u8(v: u8) -> Option<Self> {
        match v {
            0 => Some(Self::Inner),
            1 => Some(Self::Leaf),
            _ => None,
        }
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct PageHeader {
    pub page_type: u8,
    pub num_cells: u16,
    pub parent_page_id: u32,
    pub next_leaf: u32,
    pub checksum: u32,
    _padding: [u8; 2],
}

impl PageHeader {
    pub fn new(page_type: PageType) -> Self {
        Self {
            page_type: page_type as u8,
            num_cells: 0,
            parent_page_id: 0,
            next_leaf: 0,
            checksum: 0,
            _padding: [0; 2],
        }
    }

    pub fn page_type_enum(&self) -> Option<PageType> {
        PageType::from_u8(self.page_type)
    }

    pub fn serialize(&self) -> [u8; HEADER_SIZE] {
        let mut buf = [0u8; HEADER_SIZE];
        buf[0] = self.page_type;
        buf[1..3].copy_from_slice(&self.num_cells.to_be_bytes());
        buf[3..7].copy_from_slice(&self.parent_page_id.to_be_bytes());
        buf[7..11].copy_from_slice(&self.next_leaf.to_be_bytes());
        buf[11..15].copy_from_slice(&self.checksum.to_be_bytes());
        buf
    }

    pub fn deserialize(buf: &[u8; HEADER_SIZE]) -> Self {
        Self {
            page_type: buf[0],
            num_cells: u16::from_be_bytes([buf[1], buf[2]]),
            parent_page_id: u32::from_be_bytes([buf[3], buf[4], buf[5], buf[6]]),
            next_leaf: u32::from_be_bytes([buf[7], buf[8], buf[9], buf[10]]),
            checksum: u32::from_be_bytes([buf[11], buf[12], buf[13], buf[14]]),
            _padding: [buf[15], 0],
        }
    }
}

#[derive(Clone, Debug)]
pub struct Page {
    pub id: PageId,
    pub data: Box<[u8; PAGE_SIZE]>,
    pub is_dirty: bool,
}

impl Page {
    pub fn new(id: PageId, page_type: PageType) -> Self {
        let mut data = Box::new([0u8; PAGE_SIZE]);
        let header = PageHeader::new(page_type);
        let hdr = header.serialize();
        data[..HEADER_SIZE].copy_from_slice(&hdr);
        Self { id, data, is_dirty: false }
    }

    pub fn header(&self) -> PageHeader {
        let mut hdr_buf = [0u8; HEADER_SIZE];
        hdr_buf.copy_from_slice(&self.data[..HEADER_SIZE]);
        PageHeader::deserialize(&hdr_buf)
    }

    pub fn header_mut(&mut self) -> &mut PageHeader {
        unsafe { &mut *(self.data.as_mut_ptr() as *mut PageHeader) }
    }

    pub fn page_type(&self) -> PageType {
        self.header().page_type_enum().unwrap()
    }

    pub fn num_cells(&self) -> u16 {
        self.header().num_cells
    }

    // ─── Leaf cell methods ───

    fn leaf_cell_offset(&self, idx: u16) -> usize {
        let mut pos = HEADER_SIZE;
        for _i in 0..idx {
            let key_len = u16::from_be_bytes([self.data[pos], self.data[pos+1]]) as usize;
            pos += 2 + key_len;
            let val_len = u16::from_be_bytes([self.data[pos], self.data[pos+1]]) as usize;
            pos += 2 + val_len;
        }
        pos
    }

    pub fn insert_leaf_cell(&mut self, index: u16, key: &[u8], value: &[u8]) {
        let cell_size = 2 + key.len() + 2 + value.len();
        let old_pos = self.leaf_cell_offset(index);
        let end_pos = self.leaf_cell_offset(self.num_cells());
        self.data.copy_within(old_pos..end_pos, old_pos + cell_size);

        let mut pos = old_pos;
        self.data[pos..pos+2].copy_from_slice(&(key.len() as u16).to_be_bytes());
        pos += 2;
        self.data[pos..pos+key.len()].copy_from_slice(key);
        pos += key.len();
        self.data[pos..pos+2].copy_from_slice(&(value.len() as u16).to_be_bytes());
        pos += 2;
        self.data[pos..pos+value.len()].copy_from_slice(value);

        self.header_mut().num_cells += 1;
    }

    pub fn leaf_cell_at(&self, idx: u16) -> Option<(Vec<u8>, Vec<u8>)> {
        if idx >= self.num_cells() { return None; }
        let mut pos = self.leaf_cell_offset(idx);
        let key_len = u16::from_be_bytes([self.data[pos], self.data[pos+1]]) as usize;
        pos += 2;
        let key = self.data[pos..pos+key_len].to_vec();
        pos += key_len;
        let val_len = u16::from_be_bytes([self.data[pos], self.data[pos+1]]) as usize;
        pos += 2;
        let value = self.data[pos..pos+val_len].to_vec();
        Some((key, value))
    }

    pub fn remove_leaf_cell(&mut self, idx: u16) {
        if idx >= self.num_cells() { return; }
        let cell_size = {
            let mut pos = self.leaf_cell_offset(idx);
            let key_len = u16::from_be_bytes([self.data[pos], self.data[pos+1]]) as usize;
            pos += 2 + key_len;
            let val_len = u16::from_be_bytes([self.data[pos], self.data[pos+1]]) as usize;
            2 + key_len + 2 + val_len
        };
        let old_pos = self.leaf_cell_offset(idx);
        let end_pos = self.leaf_cell_offset(self.num_cells());
        self.data.copy_within(old_pos + cell_size..end_pos, old_pos);
        for i in (end_pos - cell_size)..end_pos {
            self.data[i] = 0;
        }
        self.header_mut().num_cells -= 1;
    }

    // ─── Inner node cell methods ───

    pub fn first_child_page_id(&self) -> PageId {
        if self.page_type() != PageType::Inner { return 0; }
        u32::from_be_bytes([self.data[HEADER_SIZE], self.data[HEADER_SIZE+1],
                            self.data[HEADER_SIZE+2], self.data[HEADER_SIZE+3]])
    }

    pub fn set_first_child_page_id(&mut self, id: PageId) {
        self.data[HEADER_SIZE..HEADER_SIZE+4].copy_from_slice(&id.to_be_bytes());
    }

    fn inner_cell_offset(&self, idx: u16) -> usize {
        let mut pos = HEADER_SIZE + 4;
        for _i in 0..idx {
            let key_len = u16::from_be_bytes([self.data[pos], self.data[pos+1]]) as usize;
            pos += 2 + key_len + 4;
        }
        pos
    }

    pub fn insert_inner_cell(&mut self, index: u16, key: &[u8], child_id: PageId) {
        let cell_size = 2 + key.len() + 4;
        let old_pos = self.inner_cell_offset(index);
        let end_pos = self.inner_cell_offset(self.num_cells());
        self.data.copy_within(old_pos..end_pos, old_pos + cell_size);

        let mut pos = old_pos;
        self.data[pos..pos+2].copy_from_slice(&(key.len() as u16).to_be_bytes());
        pos += 2;
        self.data[pos..pos+key.len()].copy_from_slice(key);
        pos += key.len();
        self.data[pos..pos+4].copy_from_slice(&child_id.to_be_bytes());

        self.header_mut().num_cells += 1;
    }

    pub fn inner_cell_at(&self, idx: u16) -> Option<(Vec<u8>, PageId)> {
        if idx >= self.num_cells() { return None; }
        let mut pos = self.inner_cell_offset(idx);
        let key_len = u16::from_be_bytes([self.data[pos], self.data[pos+1]]) as usize;
        pos += 2;
        let key = self.data[pos..pos+key_len].to_vec();
        pos += key_len;
        let child_id = u32::from_be_bytes([self.data[pos], self.data[pos+1],
                                            self.data[pos+2], self.data[pos+3]]);
        Some((key, child_id))
    }

    pub fn remove_inner_cell(&mut self, idx: u16) {
        if idx >= self.num_cells() { return; }
        let cell_size = {
            let pos = self.inner_cell_offset(idx);
            let key_len = u16::from_be_bytes([self.data[pos], self.data[pos+1]]) as usize;
            2 + key_len + 4
        };
        let old_pos = self.inner_cell_offset(idx);
        let end_pos = self.inner_cell_offset(self.num_cells());
        self.data.copy_within(old_pos + cell_size..end_pos, old_pos);
        for i in (end_pos - cell_size)..end_pos {
            self.data[i] = 0;
        }
        self.header_mut().num_cells -= 1;
    }

    // ─── Binary search ───

    pub fn lower_bound(&self, search_key: &[u8]) -> u16 {
        let n = self.num_cells() as usize;
        if n == 0 { return 0; }
        let mut lo = 0;
        let mut hi = n;
        while lo < hi {
            let mid = (lo + hi) / 2;
            let key = if self.page_type() == PageType::Inner {
                self.inner_cell_at(mid as u16).map(|(k, _)| k)
            } else {
                self.leaf_cell_at(mid as u16).map(|(k, _)| k)
            };
            if let Some(key) = key {
                if &key[..] < search_key { lo = mid + 1; }
                else { hi = mid; }
            } else {
                hi = mid;
            }
        }
        lo as u16
    }

    pub fn child_page_id_for_key(&self, key: &[u8]) -> PageId {
        let idx = self.lower_bound(key);
        if idx == 0 {
            self.first_child_page_id()
        } else if idx <= self.num_cells() {
            self.inner_cell_at(idx - 1).map(|(_, id)| id).unwrap_or(0)
        } else {
            self.inner_cell_at(self.num_cells() - 1).map(|(_, id)| id).unwrap_or(0)
        }
    }

    // ─── Checksum ───

    pub fn compute_checksum(&self) -> u32 {
        let mut data_copy = *self.data.clone();
        data_copy[11..15].copy_from_slice(&[0u8; 4]);
        let mut hasher = crc32fast::Hasher::new();
        hasher.update(&data_copy[..]);
        hasher.finalize()
    }

    pub fn set_checksum(&mut self) {
        let cs = self.compute_checksum();
        self.data[11..15].copy_from_slice(&cs.to_be_bytes());
    }

    pub fn verify_checksum(&self) -> bool {
        let stored = u32::from_be_bytes([self.data[11], self.data[12], self.data[13], self.data[14]]);
        stored == self.compute_checksum()
    }
}

// ─── Tests ───

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_leaf_cell_insert_and_read() {
        let mut page = Page::new(1, PageType::Leaf);
        page.insert_leaf_cell(0, b"hello", b"world");
        assert_eq!(page.num_cells(), 1);
        let (k, v) = page.leaf_cell_at(0).unwrap();
        assert_eq!(k, b"hello");
        assert_eq!(v, b"world");
    }

    #[test]
    fn test_leaf_cell_multiple_in_order() {
        let mut page = Page::new(1, PageType::Leaf);
        page.insert_leaf_cell(0, b"aaa", b"1");
        page.insert_leaf_cell(1, b"bbb", b"2");
        page.insert_leaf_cell(0, b"000", b"0");
        assert_eq!(page.num_cells(), 3);
        assert_eq!(page.leaf_cell_at(0).unwrap().0, b"000");
        assert_eq!(page.leaf_cell_at(1).unwrap().0, b"aaa");
        assert_eq!(page.leaf_cell_at(2).unwrap().0, b"bbb");
    }

    #[test]
    fn test_inner_cell_insert_and_read() {
        let mut page = Page::new(1, PageType::Inner);
        page.insert_inner_cell(0, b"key10", 42);
        assert_eq!(page.num_cells(), 1);
        let (k, child) = page.inner_cell_at(0).unwrap();
        assert_eq!(k, b"key10");
        assert_eq!(child, 42);
    }

    #[test]
    fn test_lower_bound_empty() {
        let page = Page::new(1, PageType::Leaf);
        assert_eq!(page.lower_bound(b"anything"), 0);
    }

    #[test]
    fn test_lower_bound() {
        let mut page = Page::new(1, PageType::Inner);
        page.insert_inner_cell(0, b"aaa", 10);
        page.insert_inner_cell(1, b"ccc", 20);
        page.insert_inner_cell(2, b"eee", 30);
        assert_eq!(page.lower_bound(b"aaa"), 0);
        assert_eq!(page.lower_bound(b"bbb"), 1);
        assert_eq!(page.lower_bound(b"ccc"), 1);
        assert_eq!(page.lower_bound(b"fff"), 3);
    }

    #[test]
    fn test_checksum() {
        let mut page = Page::new(1, PageType::Leaf);
        page.insert_leaf_cell(0, b"hello", b"world");
        page.set_checksum();
        assert!(page.verify_checksum());
        page.data[100] ^= 0xFF;
        assert!(!page.verify_checksum());
    }
}
