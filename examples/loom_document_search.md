# Search team documents with Loom

This example wraps [ClickHouse Loom](https://github.com/ClickHouse/loom) document search in an AI SDK C++ tool. The tool accepts a query and returns passages with their source URLs. The namespace is configured by the application.

Build the example from the repository root.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target loom_document_search
```

Set `LOOM_BASE_URL` to your Loom server's HTTPS origin and set `LOOM_ORG` and `LOOM_NAMESPACE` to a namespace with indexed documents. Set `LOOM_VIEWER_TOKEN` to a viewer token scoped to that namespace.

```sh
./build/examples/loom_document_search 'Where is the deployment runbook?'
```

The example invokes the tool through `ToolExecutor` and prints the results as JSON. It uses lexical search without a model call. To let a model use the tool, pass the same `ToolSet` to `GenerateOptions::tools`.
