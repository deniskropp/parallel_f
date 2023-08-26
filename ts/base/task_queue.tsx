import { Task } from './task'

export class TaskQueue {
    private q: Array<Task>

    constructor() {
        this.q = []
    }

    push(task: Task) {
        console.log(`TaskQueue::push(${task.fname()})`)

        this.q.push(task)
    }

    async exec() {
        console.log('TaskQueue::exec()')

        let eq = this.q

        this.q = []

        return eq.map(async task => await task.finish())
//        eq.forEach(async task => await task.finish())
    }
}
