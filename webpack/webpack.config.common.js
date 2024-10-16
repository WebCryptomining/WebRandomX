const path = require('path')
const HtmlWebpackPlugin = require('html-webpack-plugin')

module.exports = {
  entry: {
    index: path.resolve(__dirname, '../src/js/index.js')
  },
  output: {
    path: path.resolve(__dirname, '../dist'),
    filename: '[name].js',
    clean: true
  },
  context: path.resolve(__dirname, '../src'),
  plugins: [
    new HtmlWebpackPlugin({
      template: path.resolve(__dirname, '../src/index.html'),
      filename: 'index.html',
      favicon: path.resolve(__dirname, '../src/favicon.svg')
    })
  ],
  resolve: {
    fallback: {
      fs: false,
      crypto: false
    }
  },
  module: {
    rules: [
      {
        test: /\.html$/,
        loader: 'html-loader'
      },
      {
        test: /\.wasm$/,
        type: 'javascript/auto',
        loader: 'file-loader',
        options: {
          outputPath: 'wasm/',
          publicPath: 'wasm/'
        }
      }
    ]
  }
}
