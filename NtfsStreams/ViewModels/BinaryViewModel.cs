﻿using Prism.Commands;
using Prism.Mvvm;
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Input;

namespace NtfsStreams.ViewModels {
	class BinaryViewModel : BindableBase {
		private int _chunk = 1;

		public int Chunk {
			get { return _chunk; }
			set {
				if (SetProperty(ref _chunk, value)) {
					RaisePropertyChanged(nameof(HexText));
				}
			}
		}

		private int _lineWidth = 16;

		public int LineWidth {
			get { return _lineWidth; }
			set {
				if (SetProperty(ref _lineWidth, value)) {
					RaisePropertyChanged(nameof(HexText));
				}
			}
		}

		private byte[] _data;

		public byte[] Data {
			get { return _data; }
			set {
				if (SetProperty(ref _data, value)) {
					RaisePropertyChanged(nameof(HexText));
				}
			}
		}

		static Dictionary<int, Func<byte[], int, string>> _converters = new Dictionary<int, Func<byte[], int, string>> {
				{ 1, (arr, index) => arr[index].ToString("X2") },
				{ 2, (arr, index) => BitConverter.ToUInt16(arr, index).ToString("X4") },
				{ 4, (arr, index) => BitConverter.ToUInt32(arr, index).ToString("X8") },
				{ 8, (arr, index) => BitConverter.ToUInt64(arr, index).ToString("X16") },
		  };

		public int Size => Data.Length;

		public string HexText {
			get {
				var bytes = Data;
				if (bytes == null)
					return string.Empty;
				var encoding = IsASCII ? Encoding.ASCII : Encoding.Unicode;
				var count = Math.Min(Size, 1 << 16);	// limit to 64K for perf reasons

				var sb = new StringBuilder(1024);
				for (int i = 0; i < count; i += Chunk) {
					if (i % LineWidth == 0)
						sb.Append($"{i:X4}: ");
					if (i + Chunk > count)
						continue;
					sb.Append(_converters[Chunk](bytes, i)).Append(" ");
					var lastLine = i == count - Chunk;

					if (i % LineWidth == LineWidth - Chunk || lastLine) {
						// add ASCII/Unicode characters
						var str = new string(encoding.GetString(Data, lastLine ? i - (count % LineWidth) + 1 : i - LineWidth + Chunk, lastLine ? count % LineWidth : LineWidth).
							 Select(ch => char.GetUnicodeCategory(ch) == UnicodeCategory.Control || char.GetUnicodeCategory(ch) == UnicodeCategory.Format ? '.' : ch)
							 .ToArray());
						if (lastLine)
							sb.Append(new string(' ', (LineWidth - str.Length * (IsASCII ? 1 : 2)) / Chunk * (Chunk * 2 + 1)));
						sb.Append(" ").Append(str).AppendLine();
					}
				}
				return sb.ToString();
			}
		}

		private bool _rawView;

		public bool RawView {
			get { return _rawView; }
			set { SetProperty(ref _rawView, value); }
		}

		private bool _is8Bytes;

		public bool Is8Bytes {
			get { return _is8Bytes; }
			set {
				if (SetProperty(ref _is8Bytes, value) && value) {
					Is16Bytes = Is32Bytes = false;
					LineWidth = 8;
				}
			}
		}

		private bool _is16Bytes = true;

		public bool Is16Bytes {
			get { return _is16Bytes; }
			set {
				if (SetProperty(ref _is16Bytes, value) && value) {
					Is8Bytes = Is32Bytes = false;
					LineWidth = 16;
				}
			}
		}

		private bool _is32Bytes;

		public bool Is32Bytes {
			get { return _is32Bytes; }
			set {
				if (SetProperty(ref _is32Bytes, value) && value) {
					Is8Bytes = Is16Bytes = false;
					LineWidth = 32;
				}
			}
		}

		private bool _isASCII = true;

		public bool IsASCII {
			get { return _isASCII; }
			set {
				if (SetProperty(ref _isASCII, value) && value) {
					IsUTF16 = false;
					RaisePropertyChanged(nameof(HexText));
				}
			}
		}

		private bool _isUTF16;

		public bool IsUTF16 {
			get { return _isUTF16; }
			set {
				if (SetProperty(ref _isUTF16, value) && value) {
					IsASCII = false;
					RaisePropertyChanged(nameof(HexText));
				};
			}
		}

		private bool _is1Chunk = true;

		public bool Is1Chunk {
			get { return _is1Chunk; }
			set {
				if (SetProperty(ref _is1Chunk, value) && value) {
					Is2Chunk = Is4Chunk = Is8Chunk = false;
					Chunk = 1;
				}
			}
		}

		private bool _is2Chunk;

		public bool Is2Chunk {
			get { return _is2Chunk; }
			set {
				if (SetProperty(ref _is2Chunk, value) && value) {
					Is1Chunk = Is4Chunk = Is8Chunk = false;
					Chunk = 2;
				}
			}
		}

		private bool _is4Chunk;

		public bool Is4Chunk {
			get { return _is4Chunk; }
			set {
				if (SetProperty(ref _is4Chunk, value) && value) {
					Is2Chunk = Is1Chunk = Is8Chunk = false;
					Chunk = 4;
				}
			}
		}

		private bool _is8Chunk;

		public bool Is8Chunk {
			get { return _is8Chunk; }
			set {
				if (SetProperty(ref _is8Chunk, value) && value) {
					Is2Chunk = Is4Chunk = Is1Chunk = false;
					Chunk = 8;
				}
			}
		}

		public ICommand ExportCommand => new DelegateCommand(() => {
			var filename = App.MainViewModel.FileDialogService.GetFileForSave();
			if (filename == null) return;

			try {
				File.WriteAllBytes(filename, Data);
			}
			catch (Exception ex) {
				App.MainViewModel.MessageBoxService.ShowMessage(ex.Message, Constants.Title);
			}
		});
	}
}
