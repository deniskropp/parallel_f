export class Task {
    private f: Function
    private args: any[]

    constructor(f: Function, args: any[]) {
        this.f = f
        this.args = args
    }

    async finish() {
        console.log('Task::finish()')

        await this.f.apply(this.f, this.args)
    }
}
