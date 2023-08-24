import { Task } from './task'

export class TaskQueue {
    private q: Array<Task>

    constructor() {
        this.q = []
    }

    push(task: Task) {
        this.q.push(task)
    }

    async exec() {
        console.log('TaskQueue::exec()')

        while (this.q.length > 0) {
            const t = this.q.shift()

            //XXX TODO: EXECUTE TASK t
            if (t) {
                t.finish()
            }
        }
    }
}
