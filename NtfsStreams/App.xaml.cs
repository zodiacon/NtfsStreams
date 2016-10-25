using NtfsStreams.ViewModels;
using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;

namespace NtfsStreams {
	/// <summary>
	/// Interaction logic for App.xaml
	/// </summary>
	public partial class App : Application {
		MainViewModel _mainViewModel;

		protected override void OnStartup(StartupEventArgs e) {
			var vm = _mainViewModel = new MainViewModel();
			var win = new MainWindow { DataContext = vm };
			vm.MessageBoxService.SetOwner(win);
			win.Show();
		}

		protected override void OnExit(ExitEventArgs e) {
			_mainViewModel.Close();
			base.OnExit(e);
		}
	}
}
