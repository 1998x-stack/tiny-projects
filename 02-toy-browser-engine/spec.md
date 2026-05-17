# Toy Browser Engine — Specification

> Based on: Robinson (Matt Brubeck), sherpa_41, tiny-rendering-engine, mini_browser

## References

| Project | Stars | Language | Features |
|---------|-------|----------|----------|
| [Robinson](https://github.com/mbrubeck/robinson) | 1.6K | Rust | HTML/CSS parsing, style, block layout, painting |
| [sherpa_41](https://github.com/ayazhafiz/sherpa_41) | 35 | C++ (CMake) | HTML/CSS parser, style, layout, PNG/PDF output |
| [tiny-rendering-engine](https://github.com/woai3c/tiny-rendering-engine) | — | TypeScript | Full pipeline, branch-per-phase |
| [mini_browser](https://github.com/beginner-jhj/mini_browser) | — | C++17/Qt6 | HTML/CSS parsing, layout, rendering, navigation, images |

## Architecture: The Rendering Pipeline

```
  HTML Source                CSS Source
      │                          │
      ▼                          ▼
┌───────────┐            ┌───────────┐
│  HTML     │            │  CSS      │
│  Parser   │            │  Parser   │
│ (tokenizer│            │ (selector │
│  → DOM)   │            │  matching)│
└─────┬─────┘            └─────┬─────┘
      │                        │
      ▼                        ▼
┌─────────────────────────────────────┐
│         Style Tree                  │
│  (Apply CSS rules → DOM nodes)      │
│  Cascade: specificity + inheritance │
└────────────────┬────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────┐
│         Layout Tree                 │
│  Box Model: margin→border→padding   │
│  Block layout + Inline layout       │
│  Calculate x, y, width, height      │
└────────────────┬────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────┐
│         Painting                    │
│  Render boxes + text → Canvas       │
│  Output: PNG / PDF / screen         │
└─────────────────────────────────────┘
```

## Feature Specification

### 1. HTML Parser

**Input:** HTML string
**Output:** DOM tree

**Tokenization:**
- Start/end tags: `<div>`, `</div>`
- Self-closing tags: `<br/>`, `<img ... />`
- Text nodes
- Comments: `<!-- ... -->`
- Attributes: `class="foo"`, `id="bar"`, `href="..."`

**Supported Elements (subset):**
```
html, head, body, div, span, p, h1-h6,
a, img, ul, ol, li, br, hr, script, style
```

**DOM Node Structure:**
```rust
enum NodeType { Element, Text, Comment }
struct Node {
    node_type: NodeType,
    tag_name: Option<String>,        // Element only
    attributes: HashMap<String, String>,
    children: Vec<Node>,
    text: Option<String>,            // Text only
}
```

**Error Recovery:** Handle missing closing tags, unexpected end tags, unquoted attributes.

### 2. CSS Parser

**Input:** CSS string
**Output:** Stylesheet (list of rules)

**Selector Types:**
- Type selector: `div`, `p`, `h1`
- Class selector: `.container`, `.active`
- ID selector: `#main`, `#header`
- Universal selector: `*`
- Descendant: `div span`
- Child: `div > span`
- Multiple: `div.class#id`

**Supported Properties:**
| Property | Values | Default |
|----------|--------|---------|
| `color` | named, #hex, rgb() | `black` |
| `background-color` | named, #hex, rgb() | `transparent` |
| `width` / `height` | px, %, auto | `auto` |
| `display` | block, inline, none | `inline` |
| `font-size` | px, em, rem | `16px` |
| `font-family` | serif, sans-serif, monospace | `serif` |
| `font-weight` | normal, bold | `normal` |
| `margin` (top/right/bottom/left) | px, auto | `0` |
| `padding` | px | `0` |
| `border` | width style color | `none` |
| `text-align` | left, center, right | `left` |

**Specificity Calculation:** `(id-count, class-count, tag-count)`

### 3. Style Tree Construction

- Walk DOM tree
- For each element, match CSS rules
- Sort by specificity, apply cascade
- Inherit inheritable properties (color, font-*)
- Handle `display: none` → prune subtree

### 4. Layout Engine

**Box Model:**
```
┌──────────────────────────────┐
│         margin               │
│  ┌────────────────────────┐  │
│  │       border           │  │
│  │  ┌──────────────────┐  │  │
│  │  │    padding       │  │  │
│  │  │  ┌────────────┐  │  │  │
│  │  │  │  content   │  │  │  │
│  │  │  └────────────┘  │  │  │
│  │  └──────────────────┘  │  │
│  └────────────────────────┘  │
└──────────────────────────────┘
```

**Block Layout:**
- Width: default to parent content width
- Height: sum of child heights
- Vertical stacking

**Inline Layout:**
- Horizontal line filling
- Line wrapping at width boundary
- Vertical alignment: baseline

**Key Data Structures:**
```rust
struct Rect { x: f32, y: f32, width: f32, height: f32 }
struct EdgeSizes { left: f32, right: f32, top: f32, bottom: f32 }
struct Dimensions {
    content: Rect,
    padding: EdgeSizes,
    border: EdgeSizes,
    margin: EdgeSizes,
}
```

### 5. Painting / Rendering

- Walk layout tree
- Draw background colors
- Draw borders
- Render text (using system font or embedded bitmap font)
- Draw images (local files or network)
- Output: PNG image or on-screen display

### 6. Networking

- HTTP GET for external resources (CSS files, images)
- Basic URL parsing
- Resource caching

## Development Roadmap

### Phase 1: HTML Parser (Week 1-2)
- Tokenizer (state machine)
- DOM tree builder
- Test with simple HTML pages

### Phase 2: CSS Parser (Week 3)
- Tokenizer for CSS
- Rule parser
- Selector matching engine

### Phase 3: Style Tree (Week 4)
- Walk DOM + match rules
- Specificity calculation
- Property inheritance

### Phase 4: Layout Engine (Week 5-6)
- Box model data structures
- Block layout algorithm
- Inline layout algorithm

### Phase 5: Painting (Week 7)
- Render to canvas/image buffer
- Text rendering
- PNG output

### Phase 6: Networking & Polish (Week 8)
- HTTP resource loading
- Error handling
- Test with real websites (simple ones)

## Success Criteria

1. HTML parser correctly builds DOM for valid and mildly malformed HTML
2. CSS parser handles type, class, ID, and descendant selectors
3. Style tree correctly applies cascade (specificity + source order)
4. Layout engine positions block and inline elements correctly
5. Painting produces recognizable PNG rendering of test pages
6. Can render simple multi-element pages (not just single div)
