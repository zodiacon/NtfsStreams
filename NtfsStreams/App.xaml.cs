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
		protected override void OnStartup(StartupEventArgs e) {
			var vm = new MainViewModel();
			var win = new MainWindow { DataContext = vm };
			win.Show();
		}
	}
}
