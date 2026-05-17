# Toy Browser Engine — Gotchas

## 1. HTML Parsing: The Error Recovery Trap

**Problem:** Real-world HTML is rarely valid. Missing closing tags, misnested elements, implicit tags.

**Approach:** Build a spec-compliant "tree construction" algorithm (like HTML5 spec).

**Common edge cases:**
- `<p>` containing `<div>` → auto-close `<p>`
- `<li>` without `<ul>` → implicit list creation
- `<table>` missing `<tbody>` → auto-insert
- Self-closing tags: `<br>` vs `<br />`

**Gotcha:** If you try to handle every edge case, you'll never finish. Limit to a reasonable subset and document what's unsupported.

## 2. CSS Specificity Wars

**Problem:** Selector specificity calculation is subtle.

```
#id .class tag  →  (1, 1, 1)
.class tag      →  (0, 1, 1)
tag             →  (0, 0, 1)
```

**Gotcha:** Multiple class selectors vs single ID: `.a.b.c.d.e.f` has specificity `(0,6,0)`, which is STILL less than `#x` with `(1,0,0)`.

**Debug tip:** Print computed specificity for every matched rule.

## 3. The Cascade is NOT Just Specificity

**Priority order:**
1. `!important` declarations
2. Inline styles (`style="..."`)
3. ID selectors
4. Class selectors
5. Tag selectors
6. Inherited properties

**Within same specificity:** Later source wins.

**Gotcha:** `!important` reverses normal cascade. An `!important` in a user stylesheet overrides author `!important`. Don't implement `!important` in a toy engine — it's not worth the complexity.

## 4. Block vs Inline Layout Confusion

**Block elements:** Stack vertically, take full width of parent, respect width/height/margin.

**Inline elements:** Flow horizontally, wrap at line end, only horizontal margin/padding respected, vertical ignored.

**Gotcha:** An inline element containing a block element is invalid. Browsers fix it by inserting anonymous block boxes. In your toy engine, just emit a warning.

## 5. Margin Collapsing

**Problem:** Vertical margins of adjacent block boxes collapse (take the max, not the sum).

```
<div style="margin-bottom:20px">A</div>
<div style="margin-top:30px">B</div>
<!-- Gap between A and B = 30px, NOT 50px -->
```

**Gotcha:** Margin collapsing only happens for vertical margins of block boxes in normal flow. Floats, absolutely positioned elements, and flexbox children don't collapse.

## 6. Text Rendering Requires Font Metrics

**Problem:** To layout text, you need to know glyph widths, ascenders, descenders, line height.

**Solutions:**
- Use a bitmap font with fixed character widths (simplest)
- Use a system library (FreeType, CoreText)
- Pre-render ASCII glyphs to a texture atlas

**Gotcha:** Font metrics vary by font family. Loading a real `.ttf` file requires a font rendering library. For a toy engine, use monospace or a simple bitmap font.

## 7. The `display` Property Changes Everything

**Problem:** CSS `display` is not just block vs inline. Values include:
- `none` — element removed from layout completely
- `inline-block` — inline outside, block inside
- `flex`, `grid` — whole new layout modes

**Toy engine scope:** Support only `block`, `inline`, and `none`. Everything else renders as `inline`.

## 8. Z-Index and Stacking Contexts

**Problem:** Painting order (what's on top) is complex:
1. Background and borders of stacking context root
2. Negative z-index children
3. Block-level children in normal flow
4. Floats
5. Inline-level children
6. Positive z-index children

**Toy engine:** Skip z-index entirely. Paint in DOM order. Document as limitation.

## 9. Network Resource Loading

**Problem:** HTML may reference external CSS files and images.

**Issues:**
- Blocking vs non-blocking loads (CSS blocks rendering)
- Relative URL resolution
- Redirects
- Timeouts

**Toy engine:** Support local files first. Add HTTP later with simple blocking GET.

## 10. Memory Management for DOM Tree

**Problem:** The DOM tree is huge, and Rust/Go/Java have different memory models.

**Gotchas:**
- Rust: ownership + borrowing of parent-child references → use `Rc<RefCell<>>` or arena allocator
- C++: raw pointers → use `unique_ptr` for children, raw for parent
- Go/Python/JS: GC handles it, watch for leaks from orphaned subtrees

## 11. Viewport and Scrolling

**Problem:** Content may be larger than viewport.

**Toy engine:** Render to a fixed-size canvas with the viewport as clipping region. No scrolling support needed for initial version.

## 12. Why Your Layout Looks Wrong

Common causes in order of likelihood:
1. Inline element given `width` or `height` (ignored for inline)
2. Missing box-sizing: content-box vs border-box
3. Margins collapsing unexpectedly
4. Percentage height with no parent height set
5. Default browser styles not applied (your engine has no UA stylesheet!)
