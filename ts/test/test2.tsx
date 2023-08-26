import * as parallel_f from '../'


async function run() {
    const tl = new parallel_f.TaskList()

    const f_tn = async (n: number) => {
        console.log('task', n)
    }

    const task1 = parallel_f.make_task(f_tn, [1])
    const task2 = parallel_f.make_task(f_tn, [2])
    const task3 = parallel_f.make_task(f_tn, [3])

    tl.append(task1)
    tl.append(task2)
    tl.append(task3, [task1, task2])

    await tl.finish()
}


run()
