export type task_state = {
    current: 'waiting' | 'running' | 'finished'
}

export class Task {
    private f: Function
    private args: any[]

    public state: task_state

    constructor(f: Function, args: any[]) {
        this.f     = f
        this.args  = args
        this.state = { current: 'waiting' }
    }

    async finish() {
        console.log('Task::finish()')

        switch (this.state.current) {
            case 'finished':
                console.log('  -> already finished')
                return
            case 'running':
                throw new Error('running')
            case 'waiting':
                this.state.current = 'running'
        }

        await this.f.apply(this.f, this.args)

        console.log('  -> finished')

        this.state.current = 'finished'
    }

    ///
    fname(): string {
        return this.f.name.length > 0 ? this.f.name : this.f.toString()
    }
}
