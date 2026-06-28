# Kernelite Coding Guidelines

This document captures the C conventions already enforced across the Kernelite
codebase. Follow them as you write new code so the style stays consistent and we
don't have to do another sweeping refactor later.

The rules below are not arbitrary preferences — most are mechanically checked by
`clang-format`, `clang-tidy`, the compiler (`-Wall -Wextra`), or Doxygen
(`WARN_AS_ERROR = FAIL_ON_WARNINGS`). The handful that aren't automated (naming
of tags, declaration order, header/`.c` placement) are the ones this document
exists to lock in.

> **Before every commit**, run:
> ```sh
> make format        # clang-format -i on all sources
> make clang-tidy    # static analysis (linuxkernel-* are errors)
> make TEST=1 build/images/Image.elf   # full build incl. in-kernel tests
> make docs          # Doxygen; fails on any undocumented entity
> ```

---

## 1. Formatting (clang-format)

Formatting is **fully automated** by [`.clang-format`](.clang-format) (a
Linux-kernel-derived profile). Never hand-format — run `make format`. The key
parameters, for awareness:

| Setting | Value |
| --- | --- |
| Indentation | **Tabs**, `UseTab: Always`, `IndentWidth: 8`, `TabWidth: 8` |
| Column limit | **80** |
| Function brace | On its **own line** (`AfterFunction: true`) |
| Control-statement brace | **Same line** (`if (...) {`) |
| Pointer alignment | **Right** (`char *p`, not `char* p`) |
| Short ifs/loops/functions on one line | **Disallowed** |
| `SortIncludes` | **false** — include order is manual (see §4) |

Because `SortIncludes` is off, clang-format will **not** fix include ordering for
you. That is on you (§4).

---

## 2. Naming

`clang-tidy` enforces `lower_case` for variables and functions
(`readability-identifier-naming`). The rest is convention:

| Kind | Convention | Example |
| --- | --- | --- |
| Functions | `lower_snake_case` | `setup_kernel_id_map()` |
| Variables / parameters | `lower_snake_case` | `cur_phy_addr`, `mem_base` |
| Struct/union/enum field | `lower_snake_case` | `enable_grp0`, `frame_addr_47_12` |
| `typedef`'d type | `snake_case_t` | `page_table_t`, `mem_type_t` |
| Enum constants & macros | `UPPER_SNAKE_CASE` | `DEVICE`, `KERNEL_BASE`, `PAGE_SIZE` |
| Header guard | `DIR_FILE_H` (path-based) | `PAGE_TABLE_PAGE_TABLE_H` |

> **Architectural exception:** register-bitfield enum constants keep the exact
> ARM mnemonic casing where the spec uses mixed case, e.g.
> `DEVICE_nGnRnE`, `DEVICE_nGnRE` ([src/kernel/mmu/mmu.c:26](src/kernel/mmu/mmu.c#L26)).
> The leading token is still uppercased; only the spec-defined suffix is left
> as-is for readability against the manual.

### Enum constants are UPPER_SNAKE_CASE

```c
/* src/include/mem_layout/mem_layout.h:23 */
enum mem_layout_bases {
	USER_SPACE = 0x0000000000000000UL,
	KERNEL_BASE = 0xFFFF000000000000UL,
};
```

Not `user_space` / `kernel_base`.

---

## 3. Structs, unions, and enums

### 3a. Tag name must match the typedef name

When you `typedef` an aggregate, give it a tag **identical** to the typedef
name. Do **not** use an anonymous `typedef struct { ... } foo_t;`.

```c
/* src/include/page_table/page_table.h:310 — GOOD */
typedef struct page_table_t {
	page_table_entry_t entries[PTRS_PER_TABLE];
} page_table_t;

/* src/include/fdt/fdt.h:51 — GOOD */
typedef enum irq_type_t {
	IRQ_TYPE_SPI = 0x0,
	IRQ_TYPE_PPI = 0x1,
} irq_type_t;
```

```c
/* BAD — anonymous tag */
typedef enum {
	IRQ_TYPE_SPI = 0x0,
} irq_type_t;
```

The matching tag makes the type forward-declarable and keeps debugger/type
output legible.

### 3b. Drop the `_t` suffix on enums that are never used as a type

If an `enum` is only ever referenced as bare constants (you never write
`enum foo x;` or use it as a field/parameter type), it is **not** a type — drop
the `_t` and don't `typedef` it:

```c
/* src/kernel/mmu/mmu.c:26 — used only for its constants */
enum device_type {
	DEVICE_nGnRnE = 0b00,
	DEVICE_nGnRE = 0b01,
	DEVICE_nGRE = 0b10,
	DEVICE_GRE = 0b11,
};
```

Contrast with `mem_type_t`
([src/include/page_table/page_table.h:63](src/include/page_table/page_table.h#L63)),
which **is** used as a parameter type (`map_page(..., mem_type_t mem_typ)`) and
therefore is `typedef`'d with a matching tag and a `_t` suffix.

### 3c. Hardware register types: `raw` union + `_Static_assert`

Memory-mapped / system registers are modeled as a union of a `raw` integer and a
packed bitfield struct, with a size assertion immediately after:

```c
/* src/kernel/drivers/gicv3.c */
typedef union gicd_ctlr_t {
	uint32_t raw;
	struct __attribute__((packed)) {
		bool enable_grp0 : 1;
		/* ... */
	};
} gicd_ctlr_t;
_Static_assert(sizeof(gicd_ctlr_t) == 4, "GICD_CTLR must be 32 bits");
```

Always pair a new register type with its `_Static_assert(sizeof(...) == N, ...)`.

---

## 4. File declaration order

Every file follows this top-to-bottom order. Keep it.

1. File-level Doxygen comment (`@file`, `@brief`, `@author`, `@date`)
2. Header guard (`.h` only)
3. `#include "own_name.h"` first (`.c` only — the file's own header)
4. Other project includes: `#include "..."`
5. System includes: `#include <...>`
6. `#define`s
7. Types: structs / unions / enums
8. `static` (file-scope) variables
9. `static` (local) functions
10. Non-`static` (exported) functions

Include grouping example:

```c
/* src/kernel/allocator/page_allocator.c:14 */
#include "allocator/page_allocator.h"   /* own header first */

#include "utils/kprintf.h"              /* other project headers */
#include "linker/symbols.h"
#include "linker/linker_defines.h"

#include <stdint.h>                     /* system headers last */
#include <stddef.h>
#include <stdbool.h>
```

Blank lines separate the three include groups. Since `SortIncludes` is off,
clang-format preserves this exactly as written.

---

## 5. Encapsulation: headers export, `.c` files hide

**A header is the public interface.** Only put something in a `.h` if another
translation unit actually needs it.

- A type / `#define` / function / variable used **only** inside its own `.c`
  file belongs **in that `.c` file**, not the header.
- A function with external linkage that no header declares should be `static`.

### Move-to-`.c` example

`aptable_values`, `ap_values`, and `sh_values` are consumed only by
`page_table.c`, so they live there, not in the header:

```c
/* src/kernel/page_table/page_table.c:68,102,130 */
enum aptable_values { ... };
enum ap_values { ... };
enum sh_values { ... };
```

Likewise `current_el_t` is only used inside `utils.c`
([src/kernel/utils/utils.c:28](src/kernel/utils/utils.c#L28)), and the GICv3
register types live in `gicv3.c` — the header
([src/kernel/drivers/gicv3.h](src/kernel/drivers/gicv3.h)) exposes **only** the
four public functions.

### Keep-in-header example

`tcr_reg_t` stays in [src/include/mmu/mmu.h:27](src/include/mmu/mmu.h#L27)
because `main.c` uses it directly
([src/kernel/main.c:199](src/kernel/main.c#L199): `tcr_reg_t tcr;`).

### `static` example

Helpers not declared in any header get internal linkage:

```c
/* src/kernel/utils/utils.c:65 */
static bool reserve_kernel_img_pages(void);
static bool reserve_fdt_pages(const void *fdt_addr);
```

> **Watch for structural dependencies.** A symbol can *look* private (only its
> name appears in the header) yet be load-bearing because an **exported** type's
> definition depends on it. Example: `PTRS_PER_TABLE` must stay in the header
> because `page_table_t`'s array field
> ([page_table.h:312](src/include/page_table/page_table.h#L312)) is sized by it.
> Before moving anything, check it isn't referenced by another exported
> declaration in the same header.

---

## 6. Doxygen comments

Docs are generated with `WARN_AS_ERROR = FAIL_ON_WARNINGS` and
`WARN_IF_UNDOCUMENTED = YES`, so **every** documentable entity must be
documented or `make docs` fails.

### Every file starts with a `@file` block

```c
/**
 * @file page_allocator.c
 * @brief Physical page allocator implementation using a bitmap.
 *
 * Longer description...
 *
 * @author Abhin Parekadan Jose
 * @date 2026-05-16
 */
```

### Use `@brief`, not `@struct`/`@union`/`@enum NAME`

Because tags match their typedef names (§3a), an explicit `@struct NAME` command
binds the docs to the *tag* and leaves the *typedef* flagged as undocumented —
which breaks `make docs`. Just use `@brief`:

```c
/* src/include/utils/kprintf.h:15 — GOOD */
/**
 * @brief Hardware abstraction layer for serial I/O operations.
 *
 * This structure maps generic I/O requests to hardware-specific drivers.
 */
typedef struct serial_t { ... } serial_t;
```

```c
/* BAD — triggers an undocumented-typedef warning => make docs fails */
/**
 * @struct serial_t
 * @brief Hardware abstraction layer for serial I/O operations.
 */
typedef struct serial_t { ... } serial_t;
```

Document functions with `@brief`, `@param`, and `@return`; document each struct
field and enum constant with a short `/** ... */`.

---

## 7. Toolchain quick reference

| Command | Purpose |
| --- | --- |
| `make format` | Apply `clang-format` in place |
| `make clang-tidy` | Static analysis (`linuxkernel-*` = errors) |
| `make clang-tidy-fix` | Same, with auto-fixes |
| `make TEST=1 build/images/Image.elf` | Build kernel + in-kernel tests |
| `make docs` | Generate Doxygen (fails on undocumented entities) |
| `make run` | Boot under QEMU with semihosting |

Config files: [.clang-format](.clang-format), [.clang-tidy](.clang-tidy),
[docs/Doxyfile](docs/Doxyfile).
