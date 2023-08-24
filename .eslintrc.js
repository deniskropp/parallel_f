module.exports = {
    env: {
        browser: true,
        es2021: true,
    },
    extends: 'xo',
    overrides: [
        {
            env: {
                node: true,
            },
            files: [
                '.eslintrc.{js,cjs}',
            ],
            parserOptions: {
                sourceType: 'script',
            },
        },
        {
            extends: [
                'xo-typescript',
            ],
            files: [
                '*.ts',
                '*.tsx',
            ],
        },
    ],
    parserOptions: {
        ecmaVersion: 'latest',
        sourceType: 'module',
    },
    rules: {
        indent: [
            'error',
            4,
        ],
        'linebreak-style': [
            'error',
            'unix',
        ],
        'no-multiple-empty-lines': [
            'warn',
            'always',
        ],
        quotes: [
            'error',
            'single',
        ],
        semi: [
            'error',
            'never',
        ],
    },
}
