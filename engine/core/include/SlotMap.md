# SlotMap and generational handles

`SlotMap<T>` stores values in an array of slots and gives out a `Handle` instead of a pointer.

```text
Handle = { index, generation }
Slot   = { generation, optional value }
```

## Why not a pointer or a plain index?

- A **pointer** dangles when the value is destroyed or the vector grows.
- A **plain index** silently points at a *different* value once the slot is reused.

The generation fixes the second problem. Every time a slot gets a new value, its generation goes up by one. A handle is valid only when both match:

```text
Get(h) is valid  <=>  h.index < slots.size()
                      and slots[h.index].generation == h.generation
                      and slots[h.index] holds a value
```

## Example

```text
Insert(A)   -> slot 0, gen 1   -> handle {0,1}
Remove({0,1})                   slot 0 is free (gen stays 1)
Insert(B)   -> slot 0, gen 2   -> handle {0,2}
Get({0,1})  -> nullptr          old handle is stale, it can't reach B
```

## Rules

- Generation `0` is never issued, so a default-constructed handle is always invalid.
- `Clear()` empties every slot but keeps the generations, so old handles stay stale after a reset.
- After 2^32 reuses of one slot the generation wraps back to 1. In theory a handle that old could match again. This doesn't matter at game scale.
- `Handle<Tag>` is templated on a tag type, so a mesh handle can't be passed where an entity handle is expected.

Costs: lookup is one bounds check plus one compare. Freed slots are reused LIFO and the array never shrinks.
