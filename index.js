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
  //  this.clusterSequences = [new ClusterSequence(0,20,1)];
}
ClusterRecord.prototype = {
    _nextClusterQuery: 'select id, cluster, allocated, file from cluster where cluster > ? group by cluster order by cluster asc limit 1',
    _allClustersQuery: 'select id, cluster, allocated, file from cluster group by cluster order by cluster asc',
    _freeClustersQuery: 'select * from cluster where cluster > ? and allocated = 1 order by cluster asc limit 10',
    _insertFreeClustersQuery: 'insert into cluster(cluster,allocated) values(?,?)',
    _allUnallocatedQuery: "SELECT cluster from cluster WHERE allocated = 1 AND consolidated_cluster IS NULL order by cluster asc",
    constructor: ClusterRecord,
    fillInMissing: async function (conn) {
        let params = [];
        for (let cluster = 14901939; cluster <= 15260538; ++cluster) {
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
    moveClustersInDb: async function (conn, row, empty_cluster) {
        try {
            await conn.startTransaction();
            console.log('UPDATE cluster SET consolidated_cluster = ? where id = ?', [empty_cluster, row.id]);
            await conn.commit();
        } catch (ex) {}
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
    moveCluster: async function (conn, from_ofs, to_ofs) {
        //await conn.beginTransaction();
        await conn.query('UPDATE cluster SET consolidated_cluster = ? where cluster = ?', [to_ofs, from_ofs]);
       // await conn.query('UPDATE cluster SET consolidated_cluster = 0 WHERE cluster = ?', [to_ofs]);
        //await conn.commit();
    },
    moveAll: async function (conn) {
        const clusters = await this.fill(conn);
        let free_ofs = 20;
        while (clusters[free_ofs].allocated !== 0) {
            ++ free_ofs;
        }
        let want_copy_ofs = free_ofs;
        while (want_copy_ofs < clusters.length) {
            while (clusters[want_copy_ofs] !== 1) {
                ++ want_copy_ofs;
            }
            this.moveCluster(conn, want_copy_ofs-1, free_ofs++);
            while (clusters[free_ofs] !== 0) {
                ++ free_ofs;
            }
        }
    },
    move2: async function (conn) {
        let free = 20;
        let need = 0;
        let res;
        for (;;) {
            res = await conn.query(
                'SELECT c1.cluster c FROM cluster c1 '+
                'LEFT OUTER JOIN cluster c2 '+
                  'ON c1.cluster = c2.consolidated_cluster '+
                'WHERE c1.allocated = 1 AND c2.consolidated_cluster IS NULL ' +
                'GROUP BY c1.cluster ORDER BY c1.cluster ASC LIMIT 1'
            );
            free = res[0].c;

            res = await conn.query(
                'select cluster from cluster where cluster > ? and allocated = 0 and consolidated_cluster is null order by cluster asc limit 1',
                [free]
            );
            need = res[0].cluster;
            await this.moveCluster(conn, need, free);
        }
    },
};

async function main() {
    const record = new ClusterRecord();
    const conn = await mariadb.createConnection('jdbc:mariadb://root:root@localhost/resurrex');
    //conn.setAutoCommit(false);
    //await record.buildFromDb(conn);
    //await record.fillInMissing(conn);
    await record.move2(conn);
    //record.setConsolidatedClusterRefs(conn);
    conn.close();
}

main()
    .then(res => console.log(res))
    .catch(e => {
        console.error('error:', e);
        process.exit(1);
    })
    .finally(() => process.exit(0));
