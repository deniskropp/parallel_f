import * as parallel_f from '../'


async function run() {
    const tq = new parallel_f.TaskQueue()

    const f_tn = async (n: number) => {
        console.log('task', n)
    }

    for (let i = 0; i < 4; i++) {
        tq.push(parallel_f.make_task(f_tn, [i + 1]))
    }

    await tq.exec()
}


run()
