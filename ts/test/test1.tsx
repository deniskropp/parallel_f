import * as parallel_f from '../'


async function run() {
    const task = parallel_f.make_task(() => {
        console.log('task!')
    }, [])

    const tq = new parallel_f.TaskQueue()

    tq.push(task)

    await tq.exec()
}


run()
