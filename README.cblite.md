# The `cblite` Tool

`cblite` is a command-line tool for inspecting and querying [LiteCore][LITECORE] and [Couchbase Lite][CBL] databases (which are directories with a `.cblite2` extension.)

For build instructions, see [BUILDING.md](BUILDING.md). For convenience, binaries are available in the [Releases](https://github.com/couchbaselabs/couchbase-mobile-tools/releases) tab.

> NOTE: The _source code_ for the tools is Apache-licensed, as specified in [LICENSE](https://github.com/couchbaselabs/couchbase-mobile-tools/blob/master/LICENSE). However, the pre-built [cblite](https://github.com/couchbaselabs/couchbase-mobile-tools/blob/master/README.cblite.md) binaries are linked with the Enterprise Edition of Couchbase Lite, so the usage _of those pre-built binaries_ will be guided by the terms and conditions specified in Couchbase's [Enterprise License](https://www.couchbase.com/ESLA01162020).

Official support of the `cblite` tool is not yet available. This applies also to the pre-built binaries that are linked with the Enterprise Edition of Couchbase.

## Features

A selection of the available subcommands:

| Subcommand           | Purpose                                     |
|----------------------|---------------------------------------------|
| `cat`                | Display the body of one or more documents   |
| `check`              | Check for database corruption               |
| `compact`            | Compact the database or prune old revisions |
| `encrypt`, `decrypt` | Add or remove encryption (EE only)          |
| `import`, `export`   | Import or export a database to/from JSON    |
| `info`               | Display information about the database      |
| `ls`                 | List the documents in a collection          |
| `mkcoll`             | Create a collection                         |
| `mkindex`            | Create an index                             |
| `mv`                 | Move a document between collections         |
| `push`, `pull`       | Sync with a remote database                 |
| `put`                | Create or update a document                 |
| `query`, `select`     | Run [N1QL][N1QL] queries                   |
| `revs`               | List the revisions of a document            |
| `rm`                 | Delete documents                            |
| `rmindex`            | Drop an index                               |

See the [full documentation](Documentation.md)...

## Example

```
$  cblite file travel-sample.cblite2
Database:   travel-sample.cblite2/
Total size: 34MB
Documents:  31591, last sequence 31591

$  cblite ls -l --limit 10 travel-sample.cblite2
Document ID     Rev ID     Flags   Seq     Size
airline_10      1-d70614ae ---       1     0.1K
airline_10123   1-091f80f6 ---       2     0.1K
airline_10226   1-928c43f4 ---       3     0.1K
airline_10642   1-5cb6252c ---       4     0.1K
airline_10748   1-630b0443 ---       5     0.1K
airline_10765   1-e7999661 ---       6     0.1K
airline_109     1-bd546abb ---       7     0.1K
airline_112     1-ca955c69 ---       8     0.1K
airline_1191    1-28dbba6e ---       9     0.1K
airline_1203    1-045b6947 ---      10     0.1K
(Stopping after 10 docs)

$  cblite travel-sample.cblite2
(cblite) query --limit 10 ["=", [".type"], "airline"]
["_id": "airline_10"]
["_id": "airline_10123"]
["_id": "airline_10226"]
["_id": "airline_10642"]
["_id": "airline_10748"]
["_id": "airline_10765"]
["_id": "airline_109"]
["_id": "airline_112"]
["_id": "airline_1191"]
["_id": "airline_1203"]
(Limit was 10 rows)
(cblite) query --limit 10 {WHAT: [[".name"]], WHERE:  ["=", [".type"], "airline"], ORDER_BY: [[".name"]]}
["40-Mile Air"]
["AD Aviation"]
["ATA Airlines"]
["Access Air"]
["Aigle Azur"]
["Air Austral"]
["Air Caledonie International"]
["Air CaraÃ¯bes"]
["Air Cargo Carriers"]
["Air Cudlua"]
(Limit was 10 rows)
(cblite) ^D
$
```

[LITECORE]: https://github.com/couchbase/couchbase-lite-core
[CBL]: https://www.couchbase.com/products/lite
[QUERY]: https://github.com/couchbase/couchbase-lite-core/wiki/JSON-Query-Schema
[N1QL]: https://docs.couchbase.com/server/6.0/n1ql/n1ql-language-reference/index.html
