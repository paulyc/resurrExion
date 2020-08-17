//
//  index.js
//  resurrExion
//
//  Created by Paul Ciarlo on 4 August 2020
//
//  Copyright (C) 2020 Paul Ciarlo <paul.ciarlo@gmail.com>
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in all
//  copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//  SOFTWARE.
//

const mariadb = require('mariadb');

function ClusterSequence(start, end, allocated) {
    this.start = start;
    this.end = end;
    this.allocated = allocated;
}

function ClusterRecord(){
}

ClusterRecord.prototype = {
    _nextClusterQuery: 'select id, cluster, allocated from cluster where cluster > ? group by cluster order by cluster asc limit 1',
    _allClustersQuery: 'select id, cluster, allocated from cluster group by cluster order by cluster asc',
    _freeClustersQuery: 'select cluster, allocated from cluster where cluster > ? and allocated = 1 order by cluster asc limit 10',
    _insertFreeClustersQuery: 'insert into cluster(cluster,allocated) values(?,?)',
    _allUnallocatedQuery: "SELECT cluster from cluster WHERE allocated = 1 AND consolidated_cluster IS NULL order by cluster asc",
    _funJoinQuery: 'SELECT c1.cluster c FROM cluster c1 LEFT OUTER JOIN cluster c2 ON c1.cluster = c2.consolidated_cluster WHERE c1.cluster > ? AND c1.allocated = 1 AND c2.consolidated_cluster IS NULL GROUP BY c1.cluster ORDER BY c1.cluster ASC LIMIT 1',
    constructor: ClusterRecord,
    fillInMissing: async function (conn) {
        let params = [];
        for (let cluster = 20; cluster <= 15260538; ++cluster) {
            params.push([cluster,0]);
            if (params.length > 1024) {
                await conn.batch(this._insertFreeClustersQuery, params);
                params = [];
            }
        }
    },
    buildFromDb: async function(conn) {
        let rows = await conn.query(this._nextClusterQuery, [20]);
        let cluster = rows[0].cluster;
        let params = [];
        while (cluster < 15260859) {
        //while (cluster < 250) {
            rows = await conn.query(this._nextClusterQuery, [cluster]);
            let next_record = rows[0].cluster;
            while (++cluster < next_record) {
                params.push([cluster,0]);
                if (params.length > 1024) {
                    await conn.batch(this._insertFreeClustersQuery, params);
                    params = [];
                }
            }
            cluster = next_record;
        }
    },
    setConsolidatedClusterRefs: async function (conn) {
        for (const row of await conn.query(this._allUnallocatedQuery)) {
            let next_empty = await this.findNextEmptyCluster(conn);
            this.moveDBCluster(conn, row, next_empty);
        }
    },
    findNextEmptyCluster: async function (conn) {
        const res = await conn.query(this._allUnallocatedQuery);
        return res.cluster;
    },
    fill: async function (conn) {
        let offset = 0;
        const clusters = [];
        for (let offset = 0;;offset += 1000000) {
            for (const row of await conn.query('select cluster, allocated from cluster order by cluster asc limit 1000000 offset ?', [offset])) {
                clusters[row.cluster] = row.allocated;
            }
            if (offset % 1000000) {
                break;
            }
        }

        return clusters;
    },
};

async function main() {
    const conn = await mariadb.createConnection('jdbc:mariadb://root:root@localhost/resurrex');
    //const record = new ClusterRecord();
    //conn.setAutoCommit(false);
    //await record.buildFromDb(conn);
    //await record.fillInMissing(conn);

    const rows = await conn.query('SELECT c.cluster, c.allocated FROM cluster c GROUP BY c.cluster ORDER BY c.cluster ASC');
    let to_i = 0;
    let from_i = 0;
    while (rows[to_i].cluster < 116) {
        ++to_i;
    }
    while (rows[from_i].cluster < 426555) {
        ++from_i;
    }
    //let batches = []; // [[1,2],[3,4]]
    while (from_i < rows.length) {
        let to = rows[to_i].cluster;
        let from = rows[from_i].cluster;

        const batch = [from, to];
        //batches.push(batch);
        const sql = 'INSERT INTO relocate(from_cluster, to_cluster) values (?,?)';
        console.log(sql, batch);
        await conn.query(sql, batch);
        ++from_i;
        while (rows[from_i].allocated !== 0) {
            ++from_i;
        }
        ++to_i;
        while (rows[to_i].allocated !== 1) {
            ++to_i;
        }
    }
    //await conn.batch('INSERT INTO relocate(from_cluster, to_cluster) values (?,?)', batches);

	conn.close();
}

main()
    .then(res => console.log(res))
    .catch(e => {
        console.error('error:', e);
        process.exit(1);
    })
    .finally(() => process.exit(0));
