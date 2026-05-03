# simdjsonpy documentation

This directory is the source of the [mkdocs](https://www.mkdocs.org/) site and the long-form reference for the package.

## Index

| Page | What it covers |
| --- | --- |
| [Overview](index.md) | Overview |
| [Quickstart](quickstart.md) | Install, drop-in usage, first-traversal patterns. |
| **API reference** | |
| [`loads` and `dumps`](loads.md) | Drop-in replacement for `json.loads` / `json.dumps`. |
| [DOM](dom.md) | `DOMParser`, `DOMDocument`, `DOMElement`, `DOMArray`, `DOMObject`. |
| [On-Demand](ondemand.md) | `OnDemandParser`, `OnDemandValue`, lazy traversal. |
| [Streaming](streaming.md) | NDJSON / `parse_many` / `iterate_many`. |
| [Common helpers](common.md) | `PaddedString`, `validate_utf8`, `minify`, implementation switching. |
| **Operational** | |
| [Error handling](error-handling.md) | Exception hierarchy, error codes, recovery patterns. |
| [Threading](threading.md) | Free-threaded model, what is safe to share. |
| [Benchmarks](benchmarks.md) | Numbers, methodology, how to reproduce. |
| [Security and fuzzing](security.md) | Property tests, sanitizer builds, threat model. |
| [Development](development.md) | Build, test, benchmark, conventions. |

## Reading order

If you have never used the package before, read [Overview](index.md) and [Quickstart](quickstart.md) first. 
Then jump to the API page that matches your workload as most users will only need [loads](loads.md) or one of [DOM](dom.md) / [On-Demand](ondemand.md). 
Keep [Error handling](error-handling.md) handy when you start wiring it into a real codebase.

## Building the site

```bash
uv sync --group docs
mkdocs serve              # live-reloading local preview
mkdocs build --strict     # fail on broken links / nav entries
```

The configuration is in `mkdocs.yml` at the repository root. 
Any new page added under `docs/` must also be registered in the `nav` section there.

